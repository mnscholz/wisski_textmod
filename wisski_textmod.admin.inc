<?php



/**
* @author Martin Scholz
*/
function wisski_textmod_admin_paths($form_state) {

  module_load_include('inc', 'wisski_pathbuilder');

  include_once(dirname(__FILE__) . '/wisski_textmod.paths.inc');

  $model = _wisski_textmod_build_text_model_from_pb();
  variable_set('wisski_textmod_model', $model);
  
  array_walk($model['errors'], function($a) { drupal_set_message($a, 'error'); });
  array_walk($model['warnings'], function($a) { drupal_set_message($a, 'warn'); });
  
  //for pb_group: display every top group of the import PB section
  $all_groups_opt = array();
  $text_groups_opt = array();
  foreach ($model['groups'] as $gid => $g) {
    $info = wisski_pathbuilder_getPathData($gid);
    $name = wisski_pathbuilder_getName($gid);
    if ($g['top']) $all_groups_opt["gid_$gid"] = $name;
    if ($g['text_class'] !== NULL) $text_groups_opt["gid_$gid"] = $name;
  }
  
  $o_props = array();
  $rows = wisski_store_getObj()->wisski_ARCAdapter_getStore()->query('SELECT ?p  WHERE { ?p <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <http://www.w3.org/2002/07/owl#ObjectProperty> . }', 'rows');
  foreach ($rows as $r) $o_props[$r['p']] = wisski_store_getObj()->wisski_ARCAdapter_addNamespace($r['p']);
  
  $form = array(
    '#tree' => TRUE,
  );
  
  $form['path_model'] = array(
    '#type' => 'fieldset',
    '#title' => t('Path definitions'),
    );
  $form['path_model']['automodel'] = array(
    '#type' => 'checkbox',
    '#default_value' => TRUE,
    '#disabled' => TRUE,
    '#title' => t('Automatically create model from Pathbuilder'),
    '#description' => t('If checked, the following options are only informative. Changes cannot be saved.'),
  );

  if ($model['errors']) {
    $form['path_model']['errors'] = array(
      '#type' => 'markup',
      '#prefix' => '<p>',
      '#suffix' => '</p>',
      '#value' => '<strong>' . t("There are errors in the text model (see above). Correct your path definitions or texts won't be handled properly.") . '</strong>',
    );
  }
  if ($model['warnings']) {
    $form['path_model']['warnings'] = array(
      '#type' => 'markup',
      '#prefix' => '<p>',
      '#suffix' => '</p>',
      '#value' => '<strong>' . t('There are warnings in the text model (see above). It is recommended to change your path definitions.') . '</strong>',
    );
  }
  $form['path_model']['text_groups'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Text groups'),
    '#disabled' => TRUE,
    '#options' => $all_groups_opt,
    '#default_value' => array_keys($text_groups_opt),
  );

  $form['path_model']['text_paths'] = array(
    '#type' => 'fieldset',
    '#title' => t('Text paths'),
  );
  foreach ($text_groups_opt as $gid => $name) {
    $form['path_model']['text_paths']['text_group_' . $gid] = array(
      '#type' => 'fieldset',
      '#title' => t('Text group %n', array('%n' => $name)),
      '#collapsible' => TRUE,
    );

    $cl_uri = $model['groups'][substr($gid, 4)]['text_class'];
    $canonical_pid = $model['text_classes'][$cl_uri]['canonical_path'];
    
    foreach ($model['text_classes'][$cl_uri]['text_paths'] as $pid) {
      $info = $model['paths'][$pid];
      $form['path_model']['text_paths']['text_group_' . $gid]['group_' . $gid . '_text_' . $pid] = array(
        '#type' => 'item',
        '#title' => t('Path'),
        '#value' => t('%n (ID: %id, Group: %g) @c', array(
            '%id' => $pid,
            '%n' => wisski_pathbuilder_getName($pid),
            '%g' => join(' -> ', array_map(function($a) { return wisski_pathbuilder_getName($a); }, array_reverse($info['groups']))),
            '@c' =>  $canonical_pid == $pid ? (' -- ' . t('canonical')) : '',
          )
        ),
      );

    }

    $data = wisski_pathbuilder_getPathData($pid);
    $path = unserialize($data['path_array']);
    $tile_path = array('(' . $info['text_class'] . ')');
    for ($i = $info['text_class_x']; isset($path["y$i"]); $i++) {
      $tile_path[] = $path["y$i"];
      $tile_path[] = $path["x" . ($i + 1)];
    }
    $tile_path[] = $data['datatype_property'];

    $form['path_model']['text_paths']['text_group_' . $gid]['group_' . $gid . '_tilepath'] = array(
      '#type' => 'item',
      '#title' => t('Relevant path suffix'),
      '#value' => join(' -> ', $tile_path),
    );
    
  }


  $form['path_model']['default_text_groups'] = array(
    '#type' => 'fieldset',
    '#title' => t('Default text paths'),
  );
  foreach ($model['groups'] as $gid => $g) {
    if (!$g['top']) continue;
    $opts = array('<none>' => '<' . t('None') . '>');
    $def = '<none>';
    if ($g['text_class'] !== NULL) {
      $pid = $model['text_classes'][$g['text_class']]['canonical_path'];
      $opts["pid_$pid"] = wisski_pathbuilder_getName($pid) . ' -- <' . t('Self') . '>';
      $def = "pid_$pid";
    }
    foreach ($g['foreign_paths'] as $pid) {
      if ($def == '<none>') $def = "pid_$pid";
      $opts["pid_$pid"] = wisski_pathbuilder_getName($pid) . ' -- ' . wisski_pathbuilder_getName(end($model['paths'][$pid]['groups']));
    }
    $form['path_model']['default_text_groups']['default_group_' . $gid] = array(
      '#type' => 'select',
      '#title' => t('Default text path for %n', array('%n' => wisski_pathbuilder_getName($gid))),
      '#options' => $opts,
      '#default_value' => $def,
      '#disabled' => TRUE,
    );
  }


  $form['path_formats'] = array(
    '#type' => 'fieldset',
    '#title' => t('Input formats'),
  );
  
  $formats = variable_get('wisski_textmod_input_formats', array());
  $format_opts = array(
#    t('Special formats') => array(
      'undef' => t('Not available'),
      'default' => t("Node's format"),
      0 => t('System default'),
#    ),
  );
#  foreach (filter_formats() as $f) $format_opts[t('Input formats')][$f->format] = $f->name;
  foreach (filter_formats() as $f) $format_opts[$f->format] = $f->name;

  foreach ($text_groups_opt + array('default' => 'Default format') as $gid => $name) {
    $pid = $gid == 'default' ? 'default' : $model['text_classes'][$model['groups'][substr($gid, 4)]['text_class']]['canonical_path'];
    if (isset($formats[$pid])) {
      $first_format = $formats[$pid][0];
      $second_format = isset($formats[$pid][1]) ? $formats[$pid][1] : 'undef';
    } else {
      $first_format = 'undef';
      $second_format = 'undef';
    }

    $form['path_formats']["format_$gid"] = array(
      '#type' => 'select',
      '#title' => $name,
      '#options' => $format_opts,
      '#default_value' => $first_format,
      '#description' => $gid == 'default' ? t('Will be used as fallback if format for a group is undefined.') : '',
    );
    $form['path_formats']["format_${gid}_fallback"] = array(
      '#type' => 'select',
      '#title' => t('Fallback for @g', array('@g' => $name)),
      '#options' => $format_opts,
      '#default_value' => $second_format,
      '#description' => t('Fallback format if first format is Node format and it happens to be undefined or the system default format'),
    );
  }
  
  $collapse_paths_opt = array();
  $default_collapse_paths_opt = array();
  foreach ($model['paths'] as $pid => $p) {
    $collapse_paths_opt[$pid] = wisski_pathbuilder_getName($pid) . ' (' . wisski_pathbuilder_getName($p['groups'][count($p['groups']) - 1]) . ')';
    if (variable_get('wisski_textmod_collapse_foreign_text_pid_' . $pid, FALSE)) $default_collapse_paths_opt[$pid] = $pid;
  }

  $form['collapse_foreign_text'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Collapse foreign text fields'),
    '#default_value' => $default_collapse_paths_opt,
    '#options' => $collapse_paths_opt,
  );


  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
  );

  return $form;

}


function wisski_textmod_admin_paths_submit($form, $form_state) {
  $model = wisski_textmod_get_model();
  $formats = array();
  foreach ($form_state['values']['path_formats'] as $k => $v) {
    if ($v == 'undef') continue;
    if (!preg_match('/^format_(gid_(\d+)|(default))$/u', $k, $matches)) continue;
    $gidstr = $matches[1];
    $gid = empty($matches[2]) ? $matches[3] : $matches[2];
    $queue = array($v);
    $v2 = $form_state['values']['path_formats']["format_${gidstr}_fallback"];
    if ($v == 'default' && $v2 != 'undef') {
      $queue[] = $v2;
    }

    if ($gid == 'default') {
      $formats['default'] = $queue;
    } else {
      foreach ($model['text_classes'][$model['groups'][$gid]['text_class']]['text_paths'] as $pid)
        $formats[$pid] = $queue;
    }
  }
  variable_set('wisski_textmod_input_formats', $formats);
  foreach ($form_state['values']['collapse_foreign_text'] as $k => $v) {
    variable_set('wisski_textmod_collapse_foreign_text_pid_' . $k, $v != 0);
  }

}



/**
* @author Martin Scholz
*/
function wisski_textmod_admin_sync($form_state) {

  $form = array();
  $form['sync_policy'] = array(
    '#type' => 'radios',
    '#title' => t('Synchronizsation policy'),
    '#options' => array(
      's' . WISSKI_TEXTMOD_NO_SYNC => t('No synchronizsation. Do not use this unless you know what you do!'),
      's' . WISSKI_TEXTMOD_MANUAL_SYNC => t('Manual synchronizsation. You will be warned if texts are not in sync.'),
      's' . WISSKI_TEXTMOD_TRIPLE_FIRST_SYNC => t('Always take version from triple store.'),
      's' . WISSKI_TEXTMOD_NODE_FIRST_SYNC => t('Always take version from node body.'),
    ),
    '#default_value' => 's' . variable_get('wisski_textmod_sync_policy', WISSKI_TEXTMOD_NO_SYNC),
  );
  
  $triggers = array();
  if (variable_get('wisski_textmod_sync_trigger', WISSKI_TEXTMOD_SYNC_ON_LOAD | WISSKI_TEXTMOD_SYNC_ON_CRON) & WISSKI_TEXTMOD_SYNC_ON_LOAD)
    $triggers[] = 't' . WISSKI_TEXTMOD_SYNC_ON_LOAD;
  if (variable_get('wisski_textmod_sync_trigger', WISSKI_TEXTMOD_SYNC_ON_LOAD | WISSKI_TEXTMOD_SYNC_ON_CRON) & WISSKI_TEXTMOD_SYNC_ON_CRON)
    $triggers[] = 't' . WISSKI_TEXTMOD_SYNC_ON_CRON;
  
  $form['sync_triggers'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Synchronizsation triggers'),
    '#options' => array(
      't' . WISSKI_TEXTMOD_SYNC_ON_LOAD => t('Synchronizse when text node is loaded or instance is queried from triple store.'),
      't' . WISSKI_TEXTMOD_SYNC_ON_CRON => t('Periodically synchronizse when cron is run.'),
    ),
    '#default_value' => $triggers,
  );

  $form['cron_sync_fs'] = array(
    '#type' => 'fieldset',
    '#title' => t('Cron synchronizsation options'),
    '#description' => t('These options are considered only if cron synchronizsation is enabled'),
  );
  $form['cron_sync_fs']['cron_sync_limit'] = array(
    '#type' => 'select',
    '#title' => t('Limit'),
    '#description' => t('Maximum number of sync checks per cron run.'),
    '#options' => array(10 => "10", 50 => "50", 100 => "100", 200 => "200", 500 => "500", 1000 => "1000"),
    '#default_value' => variable_get('wisski_textmod_cron_sync_limit', 500),
  );
  $form['cron_sync_fs']['cron_sync_expire'] = array(
    '#type' => 'select',
    '#title' => t('Expiration'),
    '#options' => array(0 => t('Never'), 60 => t('1 minute'), 3600 => t('1 hour'), 86400 => t('1 day'), 604800 => t('1 week'), 18144000 => t('1 month')),
    '#default_value' => variable_get('wisski_textmod_cron_sync_expire', 604800),
    '#description' => t('Amount of time after which an instance will be queued for sync again'),
  );
  $form['cron_sync_fs']['cron_sync_clear_table'] = array(
    '#type' => 'checkbox',
    '#title' => t('Reset sync table'),
    '#default_value' => 0,
    '#description' => t('When checked, this will clear all sync information from the system. The information is eventually rebuild with each subsequent cron run. Only check this if you know what you are doing!'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'), 
  );

  return $form;

}



/**
* @author Martin Scholz
*/
function wisski_textmod_admin_sync_submit($form, $form_state) {
  
  variable_set('wisski_textmod_sync_policy', substr($form_state['values']['sync_policy'], 1));
  variable_set('wisski_textmod_cron_sync_limit', $form_state['values']['cron_sync_limit']);
  variable_set('wisski_textmod_cron_sync_expire', $form_state['values']['cron_sync_expire']);
  $triggers = 0;
  foreach ($form_state['values']['sync_triggers'] as $t) $triggers |= substr($t, 1);
  variable_set('wisski_textmod_sync_triggers', $triggers);

  if ($form_state['values']['cron_sync_clear_table']) {
    db_query('TRUNCATE TABLE {wisski_textmod_sync}');
  }

}

