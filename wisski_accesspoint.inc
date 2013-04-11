<?php

/* Get a (or multiple) accesspoint object(s).
*
* @see wisski_accesspoint_get()
*
* @author Martin Scholz
*/
function _wisski_accesspoint_get($aid, $col, $reload) {
  static $aps = array();
  static $all = FALSE;

  if ($reload) {
    $aps = array();
    $all = FALSE;
  }
  
  if (!$all && (!$aid || !isset($aps[$aid]))) {
    $rows = db_query('SELECT * FROM {wisski_accesspoint}' . ((!$aid) ? '' : " WHERE \"aid\" = %d"), $aid);
    while (TRUE) {//($ap = db_fetch_object($rows)) {
      $ap = db_fetch_object($rows);
      if (!$ap) break;
      $ap->settings = unserialize($ap->settings);
      $ap->endpoint = module_invoke($ap->module, 'wisski_accesspoint', 'endpoint', $ap);
      $ap->url = module_invoke($ap->module, 'wisski_accesspoint', 'url', $ap);
      $aps[$ap->aid] = $ap;
    }
    if (!$aid) $all = TRUE;
  }
  
  if (!$aid) {
    return $aps;
  } elseif (!isset($aps[$aid])) {
    return NULL;
  } elseif (!$col) {
    return $aps[$aid];
  } elseif (!isset($aps[$aid]->$col)) {
    return NULL;
  } else {
    return $aps[$aid]->$col;
  }
  
}


/* Store the data of an ap object to the DB.
*
* @see wisski_accesspoint_save()
*
* @author Martin Scholz
*/
function _wisski_accesspoint_save(&$ap) {
  
  $ap->settings = serialize($ap->settings);

  if ($ap->aid) {
    // update
    drupal_write_record('wisski_accesspoint', $ap, 'aid');
  } else {
    // insert
    drupal_write_record('wisski_accesspoint', $ap);
  }

}


/* Lists accesspoint types and type information
*
* @see wisski_accesspoint_get_types()
*
* @author Martin Scholz
*/
function _wisski_accesspoint_get_types($type, $col) {
  static $types = array();

  if (!$types) {
    foreach (module_implements('wisski_accesspoint') as $module) {
      foreach (module_invoke($module, 'wisski_accesspoint', 'types') as $t => $info) {
        $info['module'] = $module;
        $info['id'] = $t;
        $types[$t] = $info;
      }
    }
  }

  if (!$type) {
    return $types;
  } elseif (!isset($types[$type])) {
    return NULL;
  } elseif (!$col) {
    return $types[$type];
  } else {
    return $types[$type][$col];
  }

}




/* Implementation of hook_wisski_accesspoint for type local
*
* @author Martin Scholz
*/
function wisski_accesspoint_wisski_accesspoint_local($op, $ap) {

  switch ($op) {
    case 'endpoint':
      
      // return the local triple store
      drupal_load('module', 'wisski_store');
      return wisski_store_getObj()->wisski_ARCAdapter_getStore();

    case 'url':
      
      // presumes module wisski_endpoint is installed...
      global $base_root, $base_path;
      return $base_root . $base_path . 'wisski/endpoint';
    
    default:
      // local is a singleton => all other hook ops will not be called / are not interesting
      return NULL;

  }

}



/* Implementation of hook_wisski_accesspoint for type wisski
* TODO: How to enable http authentication? => ARC currently does not support it.
*
* @author Martin Scholz
*/
function wisski_accesspoint_wisski_accesspoint_wisski($op, $ap) {

  switch ($op) {
    case 'endpoint':
      
      
      include_once(drupal_get_path('module', 'wisski_store') . '/arc/ARC2.php');
      
      // set up arc config
      $cfg = array(
          'remote_store_endpoint' => wisski_accesspoint_wisski_accesspoint_wisski('url', $ap),
          'remote_store_timeout' => $ap->settings['timeout'],
          );

      return ARC2::getRemoteStore($cfg);

    case 'url':

      $url  = $ap->settings['url'];
      $url .= ((substr($url, -1) == '/') ? '' : '/') . 'wisski/endpoint';
      // add read key if specified
      if ($ap->settings['read_key']) $url .= '?key=' . urlencode($ap->settings['read_key']);
      return $url;
      
    case 'operations':
      
      // only edit needed
      return array(t('Edit') => 'admin/settings/wisski/accesspoint/' . $ap->aid . '/edit');
    
    case 'edit':
      // in contrast to endpoint type we provide special read key field

      if (!$ap->settings) {
        $ap->settings = array(
            'url' => '',
            'read_key' => '',
            'timeout' => 30,
            );
      }
      $fieldset = array();

      $fieldset['type_settings_url'] = array(
          '#type' => 'textfield',
          '#required' => TRUE,
          '#title' => t('URL of the WissKI instance'),
          '#default_value' => $ap->settings['url'],
          );
      $fieldset['type_settings_read_key'] = array(
          '#type' => 'textfield',
          '#title' => t('Read key'),
          '#default_value' => $ap->settings['read_key'],
          );
      $fieldset['type_settings_timeout'] = array(
          '#type' => 'textfield',
          '#title' => t('Connection Timeout'),
          '#default_value' => $ap->settings['timeout'],
          '#description' => t('in seconds; 0 sets standard timeout (usually 30 secs)'),
          );

      return $fieldset;

    case 'validate':

      if (! preg_match('/^[0-9]+$/', $ap->type_settings_timeout)) {
        form_set_error('type_settings_timeout', t('Only numeric values allowed in timeout.'));
      }
      return;

    case 'submit':

      return array(
          array(
            'url' => $ap->type_settings_url,
            'read_key' => $ap->type_settings_read_key,
            'timeout' => $ap['type_settings_timeout'],
            ),
          NULL
          );

    default:
      
      return NULL;

  }

}


/* Implementation of hook_wisski_accesspoint for type endpoint
*
* @author Martin Scholz
*/
function wisski_accesspoint_wisski_accesspoint_endpoint($op, $ap) {
  
  switch ($op) {
    case 'url':
      // the url is stored in the settings

      $url  = $ap->settings['url'];
      $url .= (strpos($url, '?') === FALSE) ? '?' : '&';
      if ($ap->settings['query']) {
        foreach ($ap->settings['query'] as $k => $v) {
          $url .= rawurlencode($k) . '=' . rawurlencode($v) . '&';
        }
      }
      if (substr($url, -1) == '&') $url = substr($url, 0, -1);

      return $url;

    case 'endpoint':
      // set up an ARC2 remote store

      include_once(drupal_get_path('module', 'wisski_store') . '/arc/ARC2.php');

      // set up arc config
      $cfg = array(
          'remote_store_endpoint' => wisski_accesspoint_wisski_accesspoint_endpoint('url', $ap),
          'remote_store_timeout' => $ap->settings['timeout'],
          );

      return ARC2::getRemoteStore($cfg);

    case 'operations':
      // You can only edit the settings
      
      return array(t('Edit') => 'admin/settings/wisski/accesspoint/' . $ap->aid . '/edit');

    case 'edit':
      // provide fields for the URL of the endpoint and
      // additional http query fields

      if (!$ap->settings) {
        $ap->settings = array(
            'url' => '',
            'query' => array(),
            'timeout' => 30,
            );
      }

      $fieldset = array();
      $fieldset['type_settings_url'] = array(
          '#type' => 'textfield',
          '#required' => TRUE,
          '#title' => t('Endpoint URL'),
          '#default_value' => $ap->settings['url'],
          );
      $fieldset['type_settings_query'] = array(
          '#type' => 'fieldset',
          '#title' => t('Query parameters'),
          );
      foreach ($ap->settings['query'] as $k => $v) {
        $fieldset['type_settings_query']['type_settings_query_key_' . $k] = array(
            '#type' => 'textfield',
            '#title' => check_plain($k),
            '#default_value' => $v,
            );
      }
      $fieldset['type_settings_query']['type_settings_query_new_key'] = array(
          '#type' => 'textfield',
          '#title' => t('Add new key'),
          '#description' => t('The value field will appear after saving.'),
          );
      $fieldset['type_settings_timeout'] = array(
          '#type' => 'textfield',
          '#title' => t('Connection Timeout'),
          '#default_value' => $ap->settings['timeout'],
          '#description' => t('in seconds; 0 sets standard timeout (usually 30 secs)'),
          );


      return $fieldset;

    case 'validate':
      
      if (trim($ap->type_settings_query_new_key)) {
        $new_key = 'type_settings_query_key_' . $ap->type_settings_query_new_key;
        if (isset($ap->$new_key))
          form_set_error('type_settings_query_new_key', t('Key exists already'));
      }
      if (! preg_match('/^[0-9]+$/', $ap->type_settings_timeout)) {
        form_set_error('type_settings_timeout', t('Only numeric values allowed in timeout.'));
      }

      return;

    case 'submit':

      $ap = (array) $ap;
      $settings = array('url' => $ap['type_settings_url'], 'query' => array(), 'timeout' => $ap['type_settings_timeout']);
      // get all query parameters
      foreach ($ap as $k => $v) {
        if (strpos($k, 'type_settings_query_key_') === 0) {
          $settings['query'][substr($k, 24)] = $v;
        }
      }
      // add new key with empty field if entered
      if (trim($ap['type_settings_query_new_key'])) {
        $settings['query'][trim($ap['type_settings_query_new_key'])] = '';
      }

      return array($settings, NULL);

    default:
      
      return NULL;

  }

}

