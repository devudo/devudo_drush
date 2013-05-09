<?php


/**
 * Implementation of hook_drush_command().
 *
 */
function devudo_drush_command() {
  $items = array();

  // The 'server-update' command
  $items['devudo-server-check'] = array(
    'description' => "Makes or updates a devudo server",
    'arguments' => array(
      'filling' => 'The name of the server.',
    ),
    'options' => array(
    ),
    'examples' => array(
      'drush @devudo.com server-check thinkdrop.devudo.com' => 'Make a terrible-tasting sandwich that is lacking in pickles.',
    ),
    'aliases' => array(
      'server'
    ),
    'bootstrap' => DRUSH_BOOTSTRAP_FULL, // No bootstrap at all.
  );
  return $items;
}
