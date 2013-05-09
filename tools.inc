<?php


/**
 * Helpful Exec
 */
function shop_exec($command){
  $lines = array();
  drush_log("[DEVUDO] Running: $command", "ok");
  exec($command, $lines);
  foreach ($lines as $line){
    drush_log("[EXEC] $line", 'ok');
  }
}

/**
 * Helpful Exec
 */
function shop_exec_return($command, $output = 'array'){
  $lines = array();
  drush_log("[DEVUDO] Running: $command", "ok");
  exec($command, $lines);
  return $output == 'string'? implode("\n", $lines): $lines;
}
/**
 * Helpful Exec to JSon
 */
function shop_exec_json($command, $return_string = FALSE, $label = "EXEC"){
  drush_log("[$label] Running: $command");
  drush_shell_exec($command);
  $lines = drush_shell_exec_output();
  $data_string = implode($lines);
  $data_json = json_decode($data_string);
  
  // Force return string if json did not decode
  if (!is_array($data_json) && !is_object($data_json)){
    return $data_string;
  } else {
    // If asked to return a string, return a string.
    return $return_string? $data_string: $data_json;
  }
  
  
}

/**
 * Helper to load a server node from chef server by name
 */
function shop_get_server($node_name, $options = ' -l'){
  
  $return = shop_exec_json("knife node show $node_name -Fj $options", FALSE, 'KNIFE');

  // Handle
  if (is_string($return)) {
    // If a knife node was explicitly not found, knife successfully connected.
    if (strpos($return, 'ERROR: The object you are looking for could not be found') === 0){
      return 'not found';
    }
    // Otherwise, it failed to connect for some reason.
    else {
      return drush_set_error(DRUSH_DEVUDO_ERROR, '[DEVUDO] knife failed: ' . $return);
    }
  } else {
    return $return;
  }
}