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
  return $items;
}

/**
 * Command Function for devudo-server-check
 */
function drush_devudo_server_check($name = ''){
  drush_log('HELLO DEVUDO', 'ok');

  // Check for server
  if (empty($name)){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'You must give me a server hostname.');
  }
  
  // Look for server node
  $result = db_query('SELECT nid FROM {node} WHERE type = :type AND title = :title', array(':type' => 'server', ':title' => $name));
  $nid = $result->fetchColumn(0);

  // No nid, bail out.  
  if (!$nid){
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'No server node by that name was found.');
  }
  
  $node = node_load($nid);
  drush_log("Drupal node $name found.  Looking for Chef node.", 'ok');
  
  // Look for a chef node with this name
  $server = shop_get_server($name);
  if (!empty($server->name)) {
    $ip = $server->automatic->ipaddress;
    $uptime = $server->automatic->uptime;
    drush_log("Chef node $server->name found with IP $ip. Online for $uptime", 'ok');
  
    // Since we found a server... we want to update it with our new attributes.
    // @TODO: ONLY do this if the chef node was created by us.  Let's add a
    // node[:devudo][:shopmaster] attribute.
    
    // Take Drupal $node attributes, copy to server, run chef-client

  }
  // NO CHEF MANAGED SERVER FOUND: Create server
  elseif ($server == 'not found') {
    // If none exists, call knife rackspace server create (make sure runlist is added)
    drush_log("Chef node named $name not found.  Preparing to create...", 'warning');
    
    drush_set_option('rackspace_flavor', $rackspace_flavor);
    drush_set_option('rackspace_image', $rackspace_image);
    drush_set_option('role', $role);
    drush_set_option('attributes', $attributes);
    //  
      //$data = drush_shop_provision_server_create($server_fqdn);
    
  }
  // Something else went wrong
  else {
    return drush_set_error(DRUSH_FRAMEWORK_ERROR, 'Something went wrong with knife node show.');
  }
  
  
  
  // Then, take certain attributes we want to have back in devudo.com and save
  // them to the Drupal $node
  
  
  
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
  drush_log("Hi! we're here to make a $role server called $server_fqdn!", 'ok');
  
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
  
  // run knife rackspace server create.
  //$lines = shop_exec_return($cmd);
  
  /**
   * SIMULATE
   */
  //$cmd =  "knife-rackspace-create-simulate $server_fqdn $role";
  $lines = shop_exec_return($cmd);
  
    //drush_log("[EXEC] JUST KIDDING!  Moving on...", 'ok');
    //$data['Instance ID'] =  'FAke33-eee3-47ee-b52d-RACk3RIDc24b';
    //$data['Public IP Address'] =  "111.111.111.111";
    //$data['Private IP Address'] =  "111.111.111.111";
    //return $data;
    //
    //Instance ID: 051b356e-e6b3-47ee-b52d-a0cc03fdc24b
    //Host ID: e8df6854910f156a954ed00f55b2ededf5ea22529c84a4f7165a14cd
    //Name: devmaster.localhost
    //Flavor: 512MB Standard Instance
    //Image: Ubuntu 12.04 LTS (Precise Pangolin)
    //Public DNS Name: 166-78-61-124.static.cloud-ips.com
    //Public IP Address: 166.78.61.124
    //Private IP Address: 10.182.24.87
    //Password: BoBPErQFh2Lf
    //Environment: _default
    //Run List: role[devmaster]
  
  $data = array();
  $data_lines = array_slice($lines, -12);
  foreach ($data_lines as $line){
    drush_log($line, 'ok');
    list($key, $value) = explode(": ", $line);
    $data[$key] = $value;
  }
  
  // Make sure run list is applied.
  if ($run_list && $data['Environment'] != '_fake'){
    shop_exec("knife node run_list add $server_fqdn $run_list");
  }
  return $data;
}