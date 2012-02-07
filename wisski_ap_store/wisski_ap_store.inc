<?php

/* Internal implementation of hook_wisski_accesspoint
*
* This module provides its own ap type 'store'. The store type creates a local
* triple store using ARC2 for each ap object. The user may then import RDF data
* and query it via the endpoint member.
*
* @author Martin Scholz
* 
*/
function _wisski_ap_store_wisski_accesspoint($op, $ap) {

  switch ($op) {
    case 'types':
      // provide type store
      return array(
        'store' => array(
          'name' => t('Store'),
          'description' => t('Access data in a local triple store'),
        )
      );
    
    case 'operations':
      // Provide the standard edit form plus
      // a form for import of rdf files and
      // a command to drop the store contents
      // see also the wisski_ap_store_menu hook
      return array(
        t('Edit') => 'admin/settings/wisski/accesspoint/' . $ap->aid . '/edit',
        t('Import') => 'admin/settings/wisski/accesspoint/' . $ap->aid . '/import',
        t('Reset') => 'admin/settings/wisski/accesspoint/' . $ap->aid . '/reset',
      );

    case 'url':
      
      // each store has a sparql endpoint
      global $base_root, $base_path;
      return $base_root . $base_path . 'wisski/ap_store/' . $ap->aid . '/endpoint';

    case 'endpoint':
      
      // init the ARC2 store; use same db settings as the standard WissKI
      // triple store
      // the store name will be prefixed  with the name of the standard store
      // and contain the aid so that there should be no name clash
      drupal_load('module', 'wisski_store');
      $cfg = wisski_store_getObj()->config;
      $cfg['store_name'] .= '_wisski_ap_store_' . $ap->aid;
      $store = ARC2::getStore($cfg);
      if (!$store->isSetUp()) $store->setUp();
      return $store;

    case 'delete':
      
      $store = _wisski_ap_store_wisski_accesspoint('endpoint', $ap);
      $store->drop();
      return NULL;

    case 'edit':
      
      // present a list of imported files with import dates
      $files = array();
      if (isset($ap->settings['files'])) $files = $ap->settings['files'];

      $fieldset['files_info'] = array(
        '#prefix' => '<div>',
        '#value' => t('Imported files:'),
        '#suffix' => '</div>',
      );      
      $fieldset['files'] = array(
        '#prefix' => '<div id="files-wrapper"><ul>',
        '#suffix' => '</ul></div>',
      );
      foreach ($files as $timestamp => $file) {
        $i++;
        $fieldset['files']["file_$i"] = array(
          '#prefix' => '<li>',
          '#value' => check_plain($file) . ' (' . date(DATE_RSS, $time_stamp) . ')',
          '#suffix' => '</li>',
        );
      }

      return $fieldset;

    case 'validate':
      
      return;

    case 'submit':
      
      // on add make sure there is the array of imported files in the settings
      if (!$ap->settings) $ap->settings = array('files' => array());
      return array($ap->settings, NULL);
      
  }

}



/* Form for import menu callback
*
* Provides an upload field for transmitting a file to the server
* or a text field for a URI. (via file:// files on the server can be imported)
* For big files there is a big file mode, ie. the file will
* be read and imported in chunks. This is only possible with ntriple syntax.
* For big file mode one can also specify the file encoding
*
* Requires the ap that the file will be imported into to be given.
* With $file_or_uri you can preset a certain file in the textfield.
*
* @author Martin Scholz
* 
*/
function wisski_ap_store_admin_import(&$form_state, $aid, $file_or_uri = '') {

  // There is the possibility to pass $file_or_uri via GET or POST
  // this is never actually used
  if (!$file_or_uri && $_GET['file_or_uri']) {
    $file_or_uri = $_GET['file_or_uri'];
  } elseif (!$file_or_uri && $_POST['file_or_uri']) {
    $file_or_uri = $_POST['file_or_uri'];
  }

  $form = array();
  $form['#attributes'] = array('enctype' => "multipart/form-data"); // needed for upload field

  $form['aid'] = array('#type' => 'value', '#value' => $aid,);

  $form['file_or_uri'] = array( 
      '#type' => 'textfield',
      '#title' => t('File on server or URI'),
      '#default_value' => $file_or_uri,
      );
  $form['upload'] = array(
      '#type' => 'file',
      '#title' => t('Upload local file'),
      );
  $form['import_options'] = array(
      '#type' => 'fieldset',
      '#title' => t('Import options'),
      );
  $form['import_options']['big_file_mode'] = array(
      '#type' => 'checkbox',
      '#title' => t('Import in N-Triples file mode'),
      '#description' => t('Check this for import of files too big for normal import. NOTE: The RDF data must be in N-Triples format'),
      '#default_value' => FALSE,
      '#return_value' => TRUE,
      );
  $encodings = array('\\uXXXX' => 'ASCII + \\uXXXX Unicode escapes (normal)', 'HTML-ENTITIES' => 'ASCII + HTML entities', 'UTF-8' => 'UTF-8', '' => '----------');
  $encs = mb_list_encodings();
  sort($encs);
  foreach ($encs as $e) if (!isset($encodings[strtoupper($e)])) $encodings[$e] = strtoupper($e);
  $form['import_options']['encoding'] = array(
      '#type' => 'select',
      '#title' => t('N-Triples file encoding for big file mode'),
      '#options' => $encodings,
      '#default_value' => '\\uXXXX',
      );
  $form['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Import'),
      );
  $form['cancel'] = array(
      '#type' => 'markup',
      '#value' => t('Back'),
      '#prefix' => '<a href="' . url('admin/settings/wisski/accesspoint') . '">',
      '#suffix' => '</a>',
      );

  return $form;

}


/**
* @author Martin Scholz
*/
function wisski_ap_store_admin_import_validate($form, &$form_state) {

  if ($form_state['values']['big_file_mode'] && !trim($form_state['values']['encoding'])) form_set_error('encoding', 'Please select an encoding.');

  $file = NULL;
  // if a file was uploaded it will be imported.
  // The file or uri field will then be ignored.
  if (isset($_FILES['files']) && is_uploaded_file($_FILES['files']['tmp_name']['upload'])) {
    // file uploaded => save it 
    $file = file_save_upload('upload', array());
    if (!$file) {
      form_set_error('upload', t('Error uploading file.'));
    } else {
      file_set_status($file, FILE_STATUS_TEMPORARY);
      $file = 'file://' . $file->filepath;
    }
  } else {
    // on-server file or uri
    $file = trim($form_state['values']['file_or_uri']);
    if (!$file) {
      form_set_error('', t('Please specify a file.'));
      $file = NULL;
    } elseif (is_readable($file)) {
      // if it is a local file and the file is accessable, then make a file uri
      $file = 'file://' . $file;
    } elseif (strpos($file, '://') === FALSE) {
      form_set_error('server_file', t("Could not open file @file on server for reading.", array('@file' => $file)));
      $file = NULL;
    }
  }
  // store the file location for submit handler
  $form_state['storage']['filepath'] = $file;
 
}


/**
* @author Martin Scholz
*/
function wisski_ap_store_admin_import_submit($form, &$form_state) {
  
  // restore file location
  $file = $form_state['storage']['filepath'];
  $v = $form_state['values'];

  // import the file
  list($ok, $errors) = wisski_ap_store_import($v['aid'], $file, $form_state['big_file_mode'], array('encoding' => $v['encoding']));
  if ($ok) {
    drupal_set_message(t('Import of @uri completed successfully.', array('@uri' => $file)));
  } else {
    wisski_set_msgs(t('Could not import @uri', array('@uri' => $file)), $errors);
  }

  $form_state['redirect'] = 'admin/settings/wisski/vocab_ctrl/ap_store/list';

}



/* Drops the contents of the ap with id $aid.

* This is a menu callback.
*
* @author Martin Scholz
* 
*/
function wisski_ap_store_empty($aid) {
  $ap = wisski_accesspoint_get($aid);
  if (!$ap) {
    drupal_set_message(t('No accesspoint with ID %aid.', array('%aid' => $aid)), 'error');
    return;
  }
  $ap->endpoint->reset();
  drupal_set_message(t('Accesspoint reset successfully.'));
  drupal_goto('admin/settings/wisski/accesspoint');
}


/* Import file $file_or_uri into ap with id $aid.
*
* If $big_file_mode is TRUE, the file will be read and imported in chunks as
* ARC2 will read the whole file causing out of memory errors with big files.
* This mode only supports N-Triples format for import.
* 
* $options is an array that may contain keys:
* - encoding: file encoding for big file mode
* - status_callback: callback of form function($line_no, $msgs = array())
*
* @author Martin Scholz
* 
* 
* 
*/
function wisski_ap_store_import($aid, $file_or_uri, $big_file_mode = FALSE, $options = array()) {

  $ap = wisski_accesspoint_get($aid);
  if (!$ap) return array(FALSE, array("No store with sid $sid")); 

  if ($big_file_mode) {
    // This is the big file mode
    // open file manually and read in chunks of 5000 lines until eof
    // each chunk will be passed to ARC2 for import

    $line_count = 0;

    $fh = fopen($file_or_uri, 'r');

    while (!feof($fh)) {

      $lines = '';

      for ($i = 0; !feof($fh) && $i < 5000; $i++, $line_count++) {
        $line = fgets($fh);
        
        // encode n-triples in utf-8 
        if ($options['encoding'] == '\\uXXXX') {
          // this is ascii plus unicode escapes of form \uXXXX
          // this special format is understood be ARC2
          // so needs no conversion to utf8
        } else {
          // convert into utf8 according to table
          $line = mb_convert_encoding($line, 'utf-8', $options['encoding']);
        }
        $lines .= $line . "\n";

      }
      
      // insert the chunk
      if ($lines) {

        $ap->endpoint->query('INSERT INTO <' . $ap->url . '> {' . $lines . ' }');

        if ($ap->endpoint->getErrors()) { 
          // error report if there's a callback
          if ($options['status_callback']) call_user_func($options['status_callback'], $line_count, $ap->endpoint->getErrors());
          // abort on errors
          return array(FALSE, $ap->endpoint->getErrors());  // we are array(not successful, and have error msgs)
        } 
        
        if ($options['status_callback']) {
          // status report if there's a callback
          call_user_func($options['status_callback'], $line_count, NULL);
        }
     
      } // end if ($lines)

    }
    
  } else {
    // this is normal file mode
    // just pass the file location and let ARC2 do the rest

    $ap->endpoint->query("LOAD <$file_or_uri> INTO <" . $ap->url . '>');
    if ($ap->endpoint->getErrors()) {
      // error report if there's a callback
      if ($options['status_callback']) call_user_func($options['status_callback'], $line_count, $ap->endpoint->getErrors());
      return array(FALSE, $ap->endpoint->getErrors());  // we are array(not successful, and have error msgs)
    }
  }

  // add file to list of imported files
  $ap->settings['files'][] = $file_or_uri;
  wisski_accesspoint_save($ap);
  
  return array(TRUE, array());  // we are array(successful, and have no messages)

}


/* Menu callback for sparql endpoint web interface
*
* @author Martin Scholz
* 
*/
function wisski_ap_store_endpoint($aid) {
  
  // user access is checked in menu hook
  // if user has access, she has full access
  $ap = wisski_accesspoint_get($aid);
  $config = $ap->endpoint->a;
  $config['endpoint_features'] = array('select', 'describe', 'construct', 'insert', 'delete', 'ask', 'load', 'dump');
  /* instantiation */
  $ep = ARC2::getStoreEndpoint($config);

  if (!$ep->isSetUp()) {  // should 
      $ep->setUp(); /* create MySQL tables */
  }

  /* request handling */
  $ep->go();

}

