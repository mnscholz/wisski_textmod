<?php


/**
* @author Martin Scholz
*/
function _wisski_textmod_edit_text($text, $format) {
  
  $form = array();
  
  // we do not use the standard body field generation method
  // as we cannot properly deal with a lot of settings
  $form['body'] = array(
    '#type' => 'textarea',
    '#title' => t('Text'),
    '#rows' => 30,
    '#default_value' => $text,
    '#weight' => 30,
  );
  // add the input format options
  $form['format'] = filter_form($format, 31);
  
  $form['buttons'] = array('#weight' => 90);
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );
  
  return $form;
  
}




/**
* @author Martin Scholz
*/
function wisski_textmod_edit_own_text(&$form_state, $node) {
 
  $form = _wisski_textmod_edit_text($node->body, $node->format);

  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $node->nid,
  );
  return $form;


}


/**
* @author Martin Scholz
*/
function wisski_textmod_edit_own_text_submit($form, &$form_state) {

  // the new text
  $values = $form_state['values'];
  $nid = $values['nid'];
  $text = $values['body'];
  $format = $values['format'];
    
  $node = node_load($nid, NULL, TRUE);
  $node->body = $text;
  $node->format = $format;
// TODO  $node->revision = 1;
// TODO  $node->log = '';
  
  node_save($node);
  // triples are updated via hook_nodeapi

  $form_state['redirect'] = 'node/' . $node->nid;

}



/**
* @author Martin Scholz
*/
function wisski_textmod_add_foreign_text(&$form_state, $node, $gid = NULL, $pid = NULL) {
  
  $model = wisski_textmod_get_model();

  $inst = wisski_store_getObj()->wisski_ARCAdapter_delNamespace($node->title);
  if (empty($gid)) $gid = wisski_pathbuilder_getGroupIDForIndividual($inst);
  if ($gid <= 0) {
    drupal_set_message(t('Cannot determine text modeling for node %n.', array('%n' => $node->title)), 'error');
    return "";
  }
  if (!isset($model['groups'][$gid]) || empty($model['groups'][$gid]['foreign_paths'])) {
    drupal_set_message(t('No text linkable to node %n of group %g.', array('%n' => $node->title, '%g' => $gid)), 'error');
    return "";
  }
  if (!empty($pid) && !in_array($pid, $model['groups'][$gid]['foreign_paths'])) {
    drupal_set_message(t('Path %p is not applicable to node %n.', array('%n' => $node->title, '%p' => $pid)), 'error');
    return "";
  } elseif (empty($pid)) {
    $pid = $model['groups'][$gid]['foreign_paths'][0];
  }
  $text = '';
  $format = wisski_textmod_get_format_by_path($pid);
  $path_opts = array();
  foreach ($model['groups'][$gid]['foreign_paths'] as $p) $path_opts["pid_$p"] = wisski_pathbuilder_getName($p);

  $form = _wisski_textmod_edit_text($text, $format);

  $form['pid'] = array(
    '#type' => 'select',
    '#title' => t('Text path'),
    '#options' => $path_opts,
    '#default_value' => "pid_$pid",
    '#weight' => 10,
  );

  $form['nid'] = array(
    '#type' => 'hidden',
    '#value' => $node->nid,
  );
  $form['inst'] = array(
    '#type' => 'hidden',
    '#value' => $inst,
  );
  $form['gid'] = array(
    '#type' => 'hidden',
    '#value' => $gid,
  );
  
  return $form;

}



/**
* @author Martin Scholz
*/
function wisski_textmod_add_foreign_text_submit($form, &$form_state) {

  $values = $form_state['values'];
  $text = $values['body'];
  $format = $values['format'];
  $pid = substr($values['pid'], 4); // pid_xxx !
  $nid = $values['nid'];
  $gid = $values['gid'];
  $inst = preg_split('/\s+/u', $values['inst'], 0, PREG_SPLIT_NO_EMPTY);
  $inst = $inst[0];
  
  $samepart = _wisski_pathbuilder_calculate_group_samepart($gid);
  list($ok, $inst, $text_inst) = wisski_textmod_set_foreign_text($text, $inst, '', $pid, floor(count($samepart) / 2));
  
  if (!$ok) {
    foreach ($inst as $err) drupal_set_message($err, 'error');
  } elseif ($text_inst === FALSE) { 
    drupal_set_message(t('Could not create text instance'));
  } else {
    // triples are updated via hook_nodeapi
    // but we need to store the format aswell
    $nid = wisski_store_get_nid_for_instance($text_inst);
    $node = node_load($nid, NULL, TRUE);  // get directly from db!
    $node->format = $format;
    // no extra revision
    node_save($node);
  }

  $form_state['redirect'] = $inst;

}


/**
* @author Martin Scholz
*/
function wisski_textmod_create_instance_by_text(&$form_state, $gid, $pid = NULL) {
  $model = wisski_textmod_get_model();

  $inst = '';
  if (!isset($model['groups'][$gid])) {
    drupal_set_message(t('Cannot determine text modeling for group %g.', array('%g' => $gid)), 'error');
    return "";
  }
  $is_own = $model['groups'][$gid]['text_class'] != NULL;

  if ($is_own) {
    $pid = $model['text_classes'][$model['groups'][$gid]['text_class']]['canonical_path'];
  } else {
    if (count($model['groups'][$gid]['foreign_paths']) == 0) {
      drupal_set_message(t('No text linkable to group %g.', array('%g' => $gid)), 'error');
      return;
    } elseif (!empty($pid) && !in_array($pid, $model['groups'][$gid]['foreign_paths'])) {
      drupal_set_message(t('Path %p is not applicable to group %g.', array('%g' => $gid, '%p' => $pid)), 'error');
      return;
    } elseif (empty($pid)) {
      $pid = $model['groups'][$gid]['foreign_paths'][0];
    }
  }

  $text = '';
  $format = wisski_textmod_get_format_by_path($pid);
  if ($is_own) {
    $path_opts = array("pid_$pid" => '');
  } else {
    $path_opts = array();
    foreach ($model['groups'][$gid]['foreign_paths'] as $p) $path_opts["pid_$p"] = wisski_pathbuilder_getName($p);
  }
  
  $form = _wisski_textmod_edit_text($text, $format);
  
  $form['is_own'] = array(
    '#type' => 'hidden',
    '#value' => $is_own,
  );
  $form['gid'] = array(
    '#type' => 'hidden',
    '#value' => $gid,
  );
  if (!$is_own) {
    $form['pid'] = array(
      '#type' => 'select',
      '#title' => t('Text path'),
      '#options' => $path_opts,
      '#default_value' => "pid_$pid",
    );
  } else {
    $form['pid'] = array(
      '#type' => 'hidden',
      '#value' => "pid_$pid",
    );
  }

  return $form;

}



/**
* @author Martin Scholz
*/
function wisski_textmod_create_instance_by_text_submit($form, &$form_state) {

  // the new text
  $values = $form_state['values'];
  $text = $values['body'];
  $format = $values['format'];
  $inst = preg_split('/\s+/u', $values['inst'], 0, PREG_SPLIT_NO_EMPTY);
  $inst = $inst[0];
  $is_own = $values['is_own'];
  $gid = $values['gid'];
  $pid = substr($values['pid'], 4); // pid_xxx !

  if ($is_own) {  // create a text instance
    
      $text_inst = wisski_textmod_set_own_text($text, '', $pid);
      if ($text_inst === FALSE) { 
        drupal_set_message(t('Could not create text instance'));
      } else {
        // triples are updated via _sync
        // but we need to store the format aswell
        $nid = wisski_store_get_nid_for_instance($text_inst);
        $node = node_load($nid, NULL, TRUE);  // get directly from db!
        $node->format = $format;
        // no extra revision
        node_save($node);
   
        $form_state['redirect'] = $text_inst; #'node/' . $node->nid;
      }

  } else {  // create text instance and attach it to other instance
    
    $samepart = _wisski_pathbuilder_calculate_group_samepart($gid);
    list($ok, $inst, $text_inst) = wisski_textmod_set_foreign_text($text, $inst, '', $pid, floor(count($samepart) / 2));

    if (!$ok) {
      foreach ($inst as $err) drupal_set_message($err, 'error');
    } elseif ($text_inst === FALSE) { 
      drupal_set_message(t('Could not create text instance'));
    } else {
      // triples are updated via hook_nodeapi
      // but we need to store the format aswell
      $nid = wisski_store_get_nid_for_instance($text_inst);
      $node = node_load($nid, NULL, TRUE);  // get directly from db!
      $node->format = $format;
      // no extra revision
      node_save($node);
    
      $form_state['redirect'] = $inst;
    }

  }

}




/**
* @author Martin Scholz
*/
function wisski_textmod_manual_sync_merge($form_state, $node) {
  $itext = wisski_textmod_block('view', 1);
  $itext = $itext['content'];

  $form = array();
  $form['nid'] = array(
    '#type' => 'value',
    '#value' => $node->nid,
  );
  $form['node_text'] = array(
    '#type' => 'fieldset',
    '#title' => t('Node text content'),
  );
  $form['node_text']['node_text_value'] = array(
    '#type' => 'markup',
    '#value' => check_markup($node->body, $node->format),
  );
  $form['triple_text'] = array(
    '#type' => 'fieldset',
    '#title' => t('Triple text content'),
  );
  $form['triple_text']['triple_text_value'] = array(
    '#type' => 'markup',
    '#value' => $itext,
  );
  $form['merge'] = array(
    '#type' => 'radios',
    '#title' => t('Which text version would you like to take?'),
    '#options' => array(
      'node' => t('Node text'),
      'triple' => t('Triple text'),
      'edit' => t('Something different (Edit text)'),
    ),
    '#default_value' => '',
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit'),
  );
  
  return $form;
  
}


/**
* @author Martin Scholz
*/
function wisski_textmod_manual_sync_merge_validate($form, $form_state) {
  if (empty($form_state['values']['merge'])) form_set_error('merge', t('Please choose a text version.'));
} 



/**
* @author Martin Scholz
*/
function wisski_textmod_manual_sync_merge_submit($form, &$form_state) {
  
  $node = node_load($form_state['values']['nid']);
  $m = $form_state['values']['merge'];
  switch ($m) {
    case 'node':
    case 'triple':
      include_once(dirname(__FILE__) . '/wisski_textmod.sync.inc');
      wisski_textmod_sync_node($node, $m == 'node' ? WISSKI_TEXTMOD_NODE_FIRST_SYNC : WISSKI_TEXTMOD_TRIPLE_FIRST_SYNC);
      $form_state['redirect'] = "node/" . $node->nid;
      break;
    case 'edit':
      $form_state['redirect'] = "node/" . $node->nid . '/edit_text';
      break;

  }
}
