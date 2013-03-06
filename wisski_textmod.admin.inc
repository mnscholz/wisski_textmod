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
  
  $form['automodel'] = array(
    '#type' => 'checkbox',
    '#default_value' => TRUE,
    '#disabled' => TRUE,
    '#title' => t('Automatically create model from Pathbuilder'),
    '#description' => t('If checked, the following options are only informative. Changes cannot be saved.'),
  );

  if ($model['errors']) {
    $form['errors'] = array(
      '#type' => 'markup',
      '#prefix' => '<p>',
      '#suffix' => '</p>',
      '#value' => '<strong>' . t("There are errors in the text model (see above). Correct your path definitions or texts won't be handled properly.") . '</strong>',
    );
  }
  if ($model['warnings']) {
    $form['warnings'] = array(
      '#type' => 'markup',
      '#prefix' => '<p>',
      '#suffix' => '</p>',
      '#value' => '<strong>' . t('There are warnings in the text model (see above). It is recommended to change your path definitions.') . '</strong>',
    );
  }
  $form['text_groups'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Text groups'),
    '#disabled' => TRUE,
    '#options' => $all_groups_opt,
    '#default_value' => array_keys($text_groups_opt),
  );

  $form['text_paths'] = array(
    '#type' => 'fieldset',
    '#title' => t('Text paths'),
  );
  foreach ($text_groups_opt as $gid => $name) {
    $form['text_paths']['text_group_' . $gid] = array(
      '#type' => 'fieldset',
      '#title' => t('Text group %n', array('%n' => $name)),
      '#collapsible' => TRUE,
    );

    $cl_uri = $model['groups'][substr($gid, 4)]['text_class'];
    $canonical_pid = $model['text_classes'][$cl_uri]['canonical_path'];
    
    foreach ($model['text_classes'][$cl_uri]['text_paths'] as $pid) {
      $info = $model['paths'][$pid];
      $form['text_paths']['text_group_' . $gid]['group_' . $gid . '_text_' . $pid] = array(
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

    $form['text_paths']['text_group_' . $gid]['group_' . $gid . '_tilepath'] = array(
      '#type' => 'item',
      '#title' => t('Relevant path suffix'),
      '#value' => join(' -> ', $tile_path),
    );
    
  }


  $form['default_text_groups'] = array(
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
    $form['default_text_groups']['default_group_' . $gid] = array(
      '#type' => 'select',
      '#title' => t('Default text path for %n', array('%n' => wisski_pathbuilder_getName($gid))),
      '#options' => $opts,
      '#default_value' => $def,
      '#disabled' => TRUE,
    );
  }

  return $form;

}



function wisski_textmod_admin_sync($form_state) {

  $form = array();
  $form['sync_policy'] = array(
    '#type' => 'radios',
    '#title' => t('Synchronizsation policy'),
    '#options' => array(
      's' . WISSKI_TEXTMOD_NO_SYNC => t('No synchronizsation. Use with caution!'),
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

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'), 
  );

  return $form;

}


function wisski_textmod_admin_sync_submit($form, $form_state) {
  
  variable_set('wisski_textmod_sync_policy', substr($form_state['values']['sync_policy'], 1));
  $triggers = 0;
  foreach ($form_state['values']['sync_triggers'] as $t) $triggers |= substr($t, 1);
  variable_set('wisski_textmod_sync_triggers', $triggers);

}
