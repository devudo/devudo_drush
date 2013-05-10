<?php

include('tools.inc');

/**
 * Implementation of hook_drush_command().
 *
 */
function devudo_drush_command() {
  $items = array();
  $base_url = drush_get_option('uri', $_SERVER['hostname']);

  // Idempotent Server Update/Create command.
  $items['devudo-server-check'] = array(
    'description' => "Makes or updates a devudo server",
    'arguments' => array(
      'name' => "The name of the server, as listed in http://$base_url",
    ),
    'examples' => array(
      'drush @devudo.com server-check thinkdrop.devudo.com' => 'Create or update the server named thinkdrop.devudo.com via knife rackspace server create or chef-client.',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  
  // Wrapper for knife rackspace server create.
  $items['devudo-server-destroy'] = array(
    'description' => 'Destroys a server.', 
    'arguments' => array(
      'name' =>  "The name of the server to destroy, as listed in http://$base_url",
    ),
    'aliases' => array(
      'server-destroy',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}
  
  // @TODO: Break this out into Rackspace drush ?  or chef drush?  We don't want
  // a drush command here, devudo-server-check calls the function directly.
  //// Wrapper for knife rackspace server create.
  //$items['devudo-server-create'] = array(
  //  'description' => 'Fires up a new server with knife, using the specified chef role.', 
  //  'arguments' => array(
  //    'hostname' => 'The hostname to use for the server.',
  //  ),
  //  'options' => array(
  //    'attributes' => 'A JSON string of attributes.',
  //    'rackspace_flavor' => 'The id of the Rackspace flavor you want to use. (2 through 8)',
  //    'rackspace_image' => 'The image of the Rackspace flavor you want to use.',
  //    'role' => 'The chef role to use for this server.',
  //  ),
  //  'aliases' => array(
  //    'server-create',
  //  ),
  //  'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  //);
  
/**
 * Validator for create and destroy server
 *
 * Sets three contexts
 *   drupal_server_node: The result of a node load of the server $name
 *   drupal_server_attributes: The generated attributes from devudo.com
 *   chef_server_node: The chef node as retrieved from the chef server.
 *
 * To get these contexts in later functions, use
 *
 * <?php
 *   $attributes = drush_get_context('drupal_server_node');
 */
function drush_devudo_server_destroy_validate($name){
  return drush_devudo_server_check_validate($name);
}
function drush_devudo_server_check_validate($name = '') {
  global $base_url;
  drush_log("Validating server $name", 'ok');
  
  // Look for drupal server node
  if (empty($name)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "Mising Argument: You must give me a server name.");
  }
  $result = db_query('SELECT nid FROM {node} WHERE type = :type AND title = :title', array(':type' => 'server', ':title' => $name));
  $nid = $result->fetchColumn(0);
  
  // No nid, Abort 
  if (!$nid){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "No Drupal Node Found for $name");
  }

  // Load Drupal node and attributes
  drush_log("Loading Drupal server node $name ...");
  $node = node_load($nid);
  if (empty($node->status)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "Node $nid status = 0");
  } else {
    // setting objects as contexts for later.
    drush_set_context('drupal_server_node', $node);
    drush_set_context('drupal_server_node_attributes', $node->attributes);
    drush_log("Drupal server node loaded: $name [$nid]", 'status');
  }
  
  // Load Chef Node
  drush_log("Loading Chef server node $name...", 'ok');
  $server = shop_get_server($name);
  
  // Chef node found
  if (!empty($server->name)) {
    $ip = $server->automatic->ipaddress;
    drush_log("Chef server node loaded: $name [$ip].", 'status');
    drush_set_context('chef_server_node', $server);
  }
  // Something else went wrong
  elseif ($server != 'not found') {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'Something went wrong with knife node show.');
  }
}

/**
 * Command Function for devudo-server-check
 */
function drush_devudo_server_check($name = ''){
  global $base_url;
  $chef_node = drush_get_context('chef_server_node');
  $attributes = drush_get_context('drupal_server_node_attributes');

  // If Chef node was found, pass attributes and run chef-client
  if ($chef_node) {
    $ip = $chef_node->automatic->ipaddress;
    $uptime = $chef_node->automatic->uptime;
    
    drush_log("Chef node $chef_node->name found with IP $ip.", 'ok');
    drush_log("Connecting to $chef_node->name [$ip] ...", 'status');
  
    $cmd = "uptime";
    $cmd_exec = escapeshellarg($cmd);
    shop_exec("knife ssh name:$name -x devudo $cmd_exec -a ipaddress", TRUE);
    
    // Since we found a server... we want to update it with our new attributes.
    // @TODO: ONLY do this if the chef node was created by us.  Let's add a
    // node[:devudo][:shopmaster] attribute.
    
    if ('http://' . $chef_node->normal->devudo->shopmaster == $base_url){
      drush_log("The server $name was created by us! Updating attributes...", 'ok');
      $json_path = "/tmp/$name.json";
      $attributes_json = json_encode($attributes);
      file_put_contents($json_path, $attributes_json);
      shop_exec("scp $json_path devudo@$host:~/attributes.json", TRUE);
    }
    else{
      $shopmaster = $chef_node->normal->devudo->shopmaster? $chef_node->normal->devudo->shopmaster: '<unknown>';
      drush_log("Skipping Attributes: The server $name was created by $shopmaster", 'ok');
    }
    
    // Run chef-client to update the server itself.
    $chef_client_cmd = 'sudo /usr/bin/chef-client';
    if (isset($attributes_json)) $chef_client_cmd .= ' -j attributes.json';
    
    $chef_client_cmd_exec = escapeshellarg($chef_client_cmd);
    drush_log("[DEVUDO] Running chef-client on $name:", 'ok');
    shop_exec("knife ssh name:$name -x devudo $chef_client_cmd_exec -a ipaddress", TRUE);

  } 
  // Else if no chef_node was found, nows the time to create it.
  else {
    // If none exists, call knife rackspace server create (make sure runlist is added)
    drush_log("Chef node named $name not found. Preparing to create...", 'warning');
    
    drush_set_option('rackspace_flavor', $attributes->devudo['rackspace_flavor']);
    drush_set_option('rackspace_image', $attributes->devudo['rackspace_image']);
    drush_set_option('role',  $attributes->devudo['chef_role']);
    drush_set_option('attributes', json_encode($attributes));
    drush_devudo_server_create($name);
    
    
  }
}

/**
 * Post server check.
 */
function drush_devudo_post_devudo_server_check($name){
  // Make sure the server exists
  $server = shop_get_server($name);
  drush_log('drush_devudo_post_server_check', 'ok');

  if (!empty($server->name)){
    $ip = $server->automatic->ipaddress;
    drush_log("Chef node $name found at $ip. Online for $uptime", 'ok');
    
    // Save pertinent info back to devudo.com
    $node = drush_get_context('drupal_server_node');
    $node->field_ip[LANGUAGE_NONE][0]['value'] = $ip;
    
    // Wonderful.  Rules is sending us for a loop!
    // STICKY means do NOT run devudo-server-check again.
    // The rule will skip jenkins if the node is sticky.
    $node->sticky = 1;
    node_save($node);
    drush_log("Node $node->nid updated with IP $ip", 'success');
    
  } else {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "No chef node named '$name' was found on the chef server!");
  }
  
  
}


/**
 * Command Function for devudo-server-check
 * Basic wrapper for knife rackspace server create.
 *
 * "knife rackspace server create $options --server-name $server_fqdn --image 5cebb13a-f783-4f8c-8058-c4182c724ccd -Fjson -j $attributes_json", "KNIFE CREATE", "EXEC", 'array');
 *
 * Returns the short data list that is found when chef is done.
 */
function drush_devudo_server_create($server_fqdn){
  drush_log("Creating Server: $server_fqdn", 'ok');
  
  $rackspace_flavor = drush_get_option('rackspace_flavor', '2');
  $rackspace_image = drush_get_option('rackspace_image', '5cebb13a-f783-4f8c-8058-c4182c724ccd');
  $role = drush_get_option('role', '');

  $attributes_json = drush_get_option('attributes', '{}');
  $attributes_json = escapeshellarg($attributes_json);
  
  $run_list = ($role)? "role[$role]": "";
  $run_list = ($role)? "role[$role]": "";
  $options = ($run_list)? "-r $run_list": "";
  $options .= " --flavor $rackspace_flavor ";
  
  // Not using shpo_exec here to try and avoid these logging errors.
  $cmd = "knife rackspace server create $options --server-name $server_fqdn --image $rackspace_image -Fjson -j $attributes_json";
  shop_exec($cmd, TRUE);
  
  // Make sure run list is applied.
  if ($run_list && $data['Environment'] != '_fake'){
    shop_exec("knife node run_list add $server_fqdn $run_list");
  }
}


/**
 * Command Function for devudo-server-destroy
 * Basic wrapper for knife rackspace server delete.
 *
 * Right now we just hijack knife rackspace server list and parse it's output.
 * I can't seem to find a --backend switch to let it give us JSON, and adding
 * rackspace API code to this drush package seems like doubling code, since
 * knife rackspace server list already does thir for us.
 *
 * @TODO: Figure out how to get JSON for the server list instead of parsing it
 * as strings.
 */
function drush_devudo_server_destroy($name){
  global $base_url;
  
  // If no chef node was found, don't try to delete this server.
  $chef_node = drush_get_context('chef_server_node');
  if (empty($chef_node)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'No Chef Node found to destroy.');
  }

  // Get server attributes as read from Drupal
  $attributes = drush_get_context('drupal_server_node_attributes');
  $ip = $attributes->devudo['ip'];  // CAN BE EMPTY
  
  // Get server attributes as read from Chef
  $server_ip = $chef_node->automatic->ipaddress;
  
  // If IP's don't match... Abort
  if (!empty($ip) && $server_ip != $ip) {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "IP of $name from $base_url reporting as $ip, IP of $name from Chef Server reporting as $server_ip!  Aborting.");
  }
  
  // Get rackspace data
  drush_log("Looking up rackspace instance ID for $name.", 'status');
  $instance_data = array();

  // Run $ knife rackspace server list
  $list = shop_exec_return('knife rackspace server list');
  $keys = array_filter(array_map('trim', explode("  ", array_shift($list))));
  foreach ($list as $row){
    $item = array_filter(explode(" ", $row));
    $data = array_combine($keys, $item);
    
    // If name AND ip matches, choose this data and continue.
    if ($data['Name'] == $name && $data['Public IP'] == $server_ip) {
      $instance_data = $data;
      continue;
    }
  }
  
  // If no rackspace match found?  Abort.
  if (!$instance_data){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "No rackspace server with name $name and Public IP $server_ip found.");
  }
  // Else we have data, confirm deletion
  else {
    foreach($instance_data as $key => $value){
      drush_log("    $key: $value", 'ok', 4);
    }
    if (drush_confirm('ARE YOU SURE you want to delete this server?')){
      drush_log('DESTROY DESTROY DESTROY');
      $instance_id = $instance_data['Instance ID'];
      
      // RUN: knife rackspace server delete INSTANCE-ID -P
      // -P for purge, deletes chef client and node records too.
      $cmd = "knife rackspace server delete $instance_id -P -y";
      shop_exec($cmd, TRUE);

      // Unpublish the server node:
      $node = drush_get_context('drupal_server_node');
      $node->status = 0;
      node_save($node);
      
      // Success!!
      drush_log("Rackspace server, chef client, and chef node $name successfully deleted.", 'success');
    }
  }

}