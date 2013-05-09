<?php

include('tools.inc');

/**
 * Implementation of hook_drush_command().
 *
 */
function devudo_drush_command() {
  $items = array();

  // Generic Server Check command.  If server doesn't exist yet, it creates it.
  $items['devudo-server-check'] = array(
    'description' => "Makes or updates a devudo server",
    'arguments' => array(
      'name' => 'The name of the server.',
    ),
    'options' => array(
    ),
    'examples' => array(
      'drush @devudo.com server-check thinkdrop.devudo.com' => 'Make a terrible-tasting sandwich that is lacking in pickles.',
    ),
    'aliases' => array(
      'server'
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  
  // Wrapper for knife rackspace server create.
  $items['devudo-server-create'] = array(
    'description' => 'Fires up a new server with knife, using the specified chef role.', 
    'arguments' => array(
      'hostname' => 'The hostname to use for the server.',
    ),
    'options' => array(
      'attributes' => 'A JSON string of attributes.',
      'rackspace_flavor' => 'The id of the Rackspace flavor you want to use. (2 through 8)',
      'rackspace_image' => 'The image of the Rackspace flavor you want to use.',
      'role' => 'The chef role to use for this server.',
    ),
    'aliases' => array(
      'server-create',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );
  
  // Wrapper for knife rackspace server create.
  $items['devudo-server-destroy'] = array(
    'description' => 'Destroys a server.', 
    'arguments' => array(
      'hostname' => 'The hostname of the server to destroy.',
    ),
    'aliases' => array(
      'server-destroy',
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
  );
  return $items;
}

function drush_devudo_server_check_validate($name = '') {
  global $base_url;
  drush_log("Validating $name", 'ok');
  
  // Check for drupal server node
  if (empty($name)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "Mising Argument: You must give me a server name.  It must match the title of a server node in the site $base_url.");
  }
  
  // Look for server node
  $result = db_query('SELECT nid FROM {node} WHERE type = :type AND title = :title', array(':type' => 'server', ':title' => $name));
  $nid = $result->fetchColumn(0);
  
  // No nid, bail out.  
  if (!$nid){
    global $base_url;
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'No Drupal server node by that name was found in site ' . $base_url);
  }

  // Load Drupal node and attributes  
  $node = node_load($nid);
  drush_set_context('drupal_server_node', $node);

  if (empty($node->nid)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'No way jose');
  } else {
    drush_log("Server $name loaded from $base_url/node/$id");
  }
}

/**
 * Command Function for devudo-server-check
 */
function drush_devudo_server_check($name = ''){
  $node = drush_get_context('drupal_server_node');
  $attributes = $node->attributes;

  // Look for a chef node with this name
  drush_log("Looking for chef node $name", 'ok');
  $server = shop_get_server($name);
  
  //
  if (!empty($server->name)) {
    $ip = $server->automatic->ipaddress;
    $uptime = $server->automatic->uptime;
    
    drush_log("Chef node $server->name found with IP $ip.", 'ok');
    drush_log("Connecting to $server->name [$ip] ...", 'status');
  
    $cmd = "uptime";
    $cmd_exec = escapeshellarg($cmd);
    shop_exec("knife ssh name:$name -x devudo $cmd_exec -a ipaddress", TRUE);
    
    drush_log("Server connected?");
    // Since we found a server... we want to update it with our new attributes.
    // @TODO: ONLY do this if the chef node was created by us.  Let's add a
    // node[:devudo][:shopmaster] attribute.
    
    // @TODO: Take Drupal $node attributes, copy to server, run chef-client
    
  }
  // NO CHEF MANAGED SERVER FOUND: Create server
  elseif ($server == 'not found') {
    // If none exists, call knife rackspace server create (make sure runlist is added)
    drush_log("Chef node named $name not found. Preparing to create...", 'warning');
    
    drush_set_option('rackspace_flavor', $attributes->devudo['rackspace_flavor']);
    drush_set_option('rackspace_image', $attributes->devudo['rackspace_image']);
    drush_set_option('role',  $attributes->devudo['chef_role']);
    drush_set_option('attributes', json_encode($attributes));
      
    drush_shop_provision_server_create($name);
    
    // Now the server exists and a chef node should exist for it.
    $server = shop_get_server($name);
    
    if (!empty($server->name)){
      $ip = $server->automatic->ipaddress;
      $uptime = $server->automatic->uptime;
      drush_log("Chef node $server->name created! IP $ip. Online for $uptime", 'ok');
    } else {
      return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'Server was created, but no chef node was found!');
    }
    
  }
  // Something else went wrong
  else {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'Something went wrong with knife node show.');
  }
  
  
  
  // Then, take certain attributes we want to have back in devudo.com and save
  // them to the Drupal $node
  drush_log('Loading attributes to save back to web.', 'ok');
  
  // @TODO: This fails when creating a new server!  This could be the MySQL
  // Server problem!  The Build took 15 minutes, perhaps this is too long to
  // keep the DB connection open?
  
}

function drush_devudo_post_server_check($name){
  
  drush_log("Hello post $name", 'ok');

  $node->field_ip[LANGUAGE_NONE][0]['value'] = $ip;
  
  if (!node_save($node)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'node_save() failed.');
  }
  
  drush_log("Hello from drush_devudo_post_server_check($name)", 'ok');
}


/**
 * Command Function for devudo-server-check
 * Basic wrapper for knife rackspace server create.
 *
 * "knife rackspace server create $options --server-name $server_fqdn --image 5cebb13a-f783-4f8c-8058-c4182c724ccd -Fjson -j $attributes_json", "KNIFE CREATE", "EXEC", 'array');
 *
 * Returns the short data list that is found when chef is done.
 */
function drush_shop_provision_server_create($server_fqdn){
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
  
  $node = drush_get_context('drupal_server_node');

  $attributes = $node->attributes;
  $ip = $attributes->devudo['ip'];
  
  // Find a chef node with this name
  $server = shop_get_server($name);
  $server_ip = $server->automatic->ipaddress;
  $instance_data = array();
  
  // If IP's don't match... Abort
  if ($server->automatic->ipaddress != $ip) {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "IP of $name from $base_url reporting as $ip, IP of $name from Chef Server reporting as $server_ip!  Aborting.");
  }
  
  // If we have a server object...
  if (!empty($server->name)) {
    drush_log("Chef node $name found.  Looking up rackspace instance ID.", 'ok');
    
    // Get rackspace data
    $list = shop_exec_return('knife rackspace server list');
    $keys = array_filter(array_map('trim', explode("  ", array_shift($list))));
    foreach ($list as $row){
      $item = array_filter(explode(" ", $row));
      $data = array_combine($keys, $item);
      
      // If name AND ip matches, choose this data and continue.
      if ($data['Name'] == $name && $data['Public IP'] == $ip) {
        $instance_data = $data;
        continue;
      }
    }
    
    // If no rackspace match found?  Abort.
    if (!$instance_data){
      return drush_set_error(DRUSH_FRAMEWORK_ERROR, "No rackspace server with name $name and Public IP $ip found.");
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
        $node->status = 0;
        node_save($node);
        
        // Success!!
        drush_log("Rackspace server, chef client, and chef node $name successfully deleted.", 'success');
        
        return ;
      }
    }
  }
  // Else if there is no Chef Node with that name
  elseif ($server == 'not found') {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, "No chef node with name $name."); 
  }
  // Else if something happened with knife...
  else {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'Something is wrong with knife');
  }
  
}