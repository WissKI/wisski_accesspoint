<?php


/** Lists all accesspoints
*
* @author Martin Scholz
*/
function wisski_accesspoint_admin_list() {

  $aps = wisski_accesspoint_get();

  $aps_list = array();
  foreach ($aps as $ap) {
    $aid = $ap->aid;
    $item = array();
    $item[] = check_plain($ap->name);
    $item[] = check_plain(wisski_accesspoint_get_types($ap->type, 'name'));
    if (strlen($ap->url) > 50) {
      $item[] = '<a href="' . $ap->url . '" title="' . $ap->url . '">' . check_plain(substr($ap->url, 0, 47) . '...')  . '</a>';
    } else {
      $item[] = '<a href="' . $ap->url . '">' . check_plain($ap->url) . '</a>';
    }
    $ops = '';
    $op_list = (array) module_invoke($ap->module, 'wisski_accesspoint', 'operations', $ap);
    foreach($op_list as $op_title => $op_url) {
      $ops .= '<a href="' . url($op_url) . '">' . $op_title . '</a> ';
    }
    $item[] = $ops;
    $aps_list[] = $item;
  }

  usort($aps_list, function($a, $b) { $c = strcmp($a[1], $b[1]); return ($c) ? $c : strcmp($a[0], $b[0]); });

  $output = theme('table', array(t('Name'), t('Type'), t('URL'), t('Operations')), $aps_list);
  return $output;

}


/* Page for choosing a type before the add form
*
* @author Martin Scholz
*/
function wisski_accesspoint_admin_add() {

  $form = array();
  $form['dl'] = array(
      '#type' => 'markup',
      '#prefix' => '<dl>',
      '#suffix' => '</dl>',
      );
  $types = wisski_accesspoint_get_types();
  uasort($types, function($a, $b) { return strcmp($a['name'], $b['name']); });
  foreach ($types as $type => $info) {
    if ($type == 'local') continue; // singleton!
    $form['dl'][$type] = array(
        '#type' => 'markup',
        '#prefix' => '<li>',
        '#value' => '<dt><a href="' . url('admin/settings/wisski/accesspoint/add/' . $type) . '">' . $info['name'] . '</a></dt><dd>' . $info['description'] . '</dd>',
        '#suffix' => '</li>',
        );
  }

  return $form;

}


/* Build the add/edit form for an accesspoint.
*
* Combine the form from fixed fields and type dependent fields
*
* @author Martin Scholz
*/
function wisski_accesspoint_admin_edit(&$form_state, $aid = NULL, $type = '') {
  
  $edit = $aid != NULL;

  $types = wisski_accesspoint_get_types();
  if (!isset($types[$type])) $type = '';

  if ($edit) {
    $ap = wisski_accesspoint_get($aid);
    if (!$type) $type = $ap->type;
  } else {
    $ap = (object) array(
      'aid' => NULL,
      'name' => '',
      'type' => $type,
      'module' => ($type) ? $types[$type]['module'] : '',
      'settings' => NULL,
    );
  }
  
  $types_opt = (!$type) ? array('' => '<' . t('Select type') . '>') : array();
  foreach ($types as $t => $info) {
    if ($t == 'local') continue;
    $types_opt[$t] = $info['name'];
  }

  // set multipart, in case the type settings section has a file field
  $form['#attributes'] = array('enctype' => "multipart/form-data");
  
  // store aid, module and settings in hidden fields
  $form['aid'] = array('#type' => 'value', '#value' => $ap->aid);
  $form['module'] = array('#type' => 'value', '#value' => $ap->module);
  $form['settings'] = array('#type' => 'value', '#value' => $voc->settings);
  
  $form['name'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#default_value' => $ap->name,
    '#required' => TRUE,
  );
  $form['type'] = array(
    '#type' => 'select',
    '#title' => 'Type',
    '#options' => $types_opt,
    '#default_value' => $ap->type,
    '#required' => TRUE,
    '#disabled' => $edit,
    '#ahah' => array(
      'path' => 'wisski/accesspoint/js/edit',
      'wrapper' => 'type-settings-wrapper',
      'method' => 'replace',
      'effect' => 'fade',
    ),
  );
  
  $form['type_settings'] = array(
    '#type' => 'fieldset',
    '#title' => t('@type dependent settings', array('@type' => (!$ap->type) ? t('Type') : $types[$ap->type]['name'])),
    '#description' => ($type) ? $types[$type]['description'] : t('No type selected. Please select a type.'),
    '#prefix' => '<div id="type-settings-wrapper">',
    '#suffix' => '</div>',
  );

  $form['type_settings'] += (array) module_invoke($ap->module, 'wisski_accesspoint', 'edit', $ap);

  $form['buttons'] = array(
    '#prefix' => '<div>',
    '#suffix' => '</div>',
  );
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#executes_submit_callback' => TRUE,
    '#validate' => array('wisski_accesspoint_admin_edit_validate'),
    '#submit' => array('wisski_accesspoint_admin_edit_submit'),
  );
  if ($edit) {
    $form['buttons']['delete'] = array(
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#submit' => array('wisski_accesspoint_admin_edit_delete'),
    );
  }
  $form['buttons']['cancel'] = array(
    '#prefix' => '<a href="..">',
    '#value' => t('Cancel'),
    '#suffix' => '</a>',
  );
  
  return $form;
  
}


/* This function gets called on change of the type field in edit form.
*
* @author Martin Scholz
*/
function wisski_accesspoint_admin_edit_js() {

  $form_state = array('storage' => NULL, 'submitted' => FALSE, 'rebuild' => TRUE);
  $form_build_id = $_POST['form_build_id'];
  $form = form_get_cache($form_build_id, $form_state);

  $args = $form['#parameters']; 
  $form_id = array_shift($args);  
  $form_state['values'] = $form_state['post'] = $form['#post'] = $_POST;  
  $form['#programmed'] = $form['#redirect'] = FALSE;
  $args[2] = $form_state['post']['type'];  // pass type to form hook as arg ($type is arg 1 in form hook; 1 must be added to args index)

// we don't want to submit anything! don't call: drupal_process_form($form_id, $form, $form_state);
  $form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);

  $settings = $form['type_settings'];
  unset($settings['#prefix'], $settings['#suffix']);

  $output = drupal_render($settings);

  print drupal_json(array('status' => true, 'data' => $output));
  exit();

}



/**
* @author Martin Scholz
*/
function wisski_accesspoint_admin_edit_validate($form, &$form_state) {
  
  $ap = (object) $form_state['values'];
  
  // name should be unique => check if name already exists
  $res = db_query("SELECT * FROM {wisski_accesspoint} WHERE `name` = '%s'", $ap->name);
  $other_ap = db_fetch_object($res);
  if ($other_ap && $other_ap->aid != $ap->aid) {
    form_set_error('name', 'There already exists an accesspoint with that name. Please choose a different one.');
  }
  
  // validte the type dependent part
  module_invoke($ap->module, 'wisski_accesspoint', 'validate', $ap);

}


function wisski_accesspoint_admin_edit_submit($form, &$form_state) {

  $ap = (object) $form_state['values'];
  
  // submit the type dependent parts
  list($settings, $redirect) = module_invoke($ap->module, 'wisski_accesspoint', 'submit', $ap);

  $ap->settings = $settings;

  wisski_accesspoint_save($ap);
  
  if ($redirect) $form_state['redirect'] = $redirect;
  else $form_state['redirect'] = 'admin/settings/wisski/accesspoint';

}


/* Submit handler for delete button in edit form
*
* @author Martin Scholz
*/
function wisski_accesspoint_admin_edit_delete($form, &$form_state) {

  $form_state['redirect'] = 'admin/settings/wisski/accesspoint/' . $form_state['values']['aid'] . '/delete';

}


/**
* @author Martin Scholz
*/
function wisski_accesspoint_admin_delete($aid) {

  $ap = wisski_accesspoint_get($aid);

  // display a confirmation message before deleting
  return drupal_get_form(
      'wisski_helper_delete_template',
      $ap->name,
      "admin/settings/wisski/accesspoint/$aid/edit", 
      "admin/settings/wisski/accesspoint",
      'wisski_accesspoint_delete',
      $aid
  );

}

