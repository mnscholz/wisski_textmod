<?php


/**
* @author Martin Scholz
*
* 
*/
function _wisski_textmod_sync($inst, $node, $sync_policy) {
  if ($sync_policy == WISSKI_TEXTMOD_DEFAULT_SYNC) $sync_policy = variable_get('wisski_textmod_sync_policy', WISSKI_TEXTMOD_MANUAL_SYNC);
  //$sync_policy &= (WISSKI_TEXTMOD_NO_SYNC | WISSKI_TEXTMOD_MANUAL_SYNC | WISSKI_TEXTMOD_TRIPLE_FIRST_SYNC | WISSKI_TEXTMOD_NODE_FIRST_SYNC);

  include_once(dirname(__FILE__) . '/wisski_textmod.paths.inc');
  list($ok, $texts) = wisski_textmod_get_own_texts($inst);
  if (!$ok) {
    return array(FALSE, $texts);
  }

  // TODO multiple texts
  // this handles single texts only
  if (count($texts) > 1) {
    // warn if there are more than one text paths and treat like manual sync
    drupal_set_message(t('Multiple texts for instance !i not supported at the moment. Please manually resolve.', array('!i' => l(check_plain($inst), $inst))), 'error');
    _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_NOT_IN_SYNC);
  }

  if (empty($texts)) {
    $pid = NULL;
    $text = '';
  } else {
    $pid = $texts[0]['pid'];
    $text = $texts[0]['text'];
  }

  
  // the easy case: if both texts are equal, we need not do anything
  if ($text == $node->body) {
    _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_IN_SYNC);
    return array(WISSKI_TEXTMOD_IN_SYNC, $text, $text);
  }
  
  // both texts differ, choose strategy
  switch ($sync_policy) {
    
    // the text version in the triple store will be taken
    case WISSKI_TEXTMOD_TRIPLE_FIRST_SYNC:
      
      $node->body = $text;
      $node->revision = 1;
      $node->log = 'Automatic sync with triple store.';
      drupal_set_message(t('The text node content got synced with the triple store.')); 
      node_save($node);
      node_load($arg1, NULL, TRUE); // if we don't reload with cache reset, subsequent calls to node_load will return the unchanged node!

      _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_IN_SYNC);
      return array(WISSKI_TEXTMOD_IN_SYNC, $text, $text);
    
    // the text version in the node body will be taken 
    case WISSKI_TEXTMOD_NODE_FIRST_SYNC:

      include_once(dirname(__FILE__) . '/wisski_textmod.paths.inc');
      if (!empty($text)) {
        $tmp = _wisski_textmod_delete_own_text($inst, $text, $pid);
        if (!$tmp) {
          drupal_set_message(t('Could not sync: Could not delete old text from triple store.'), 'error');
          _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_NOT_IN_SYNC);
          return array(WISSKI_TEXTMOD_NOT_IN_SYNC, $text, $node->body); 
        }
      }
      if (!empty($node->body)) {
        $tmp = _wisski_textmod_set_own_text($inst, $node->body, $pid);
        if (!$tmp) {
          drupal_set_message(t('Could not sync: Could not write new text to triple store.'), 'error');
          _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_NOT_IN_SYNC);
          return array(WISSKI_TEXTMOD_NOT_IN_SYNC, $text, $node->body); 
        }
      }
      drupal_set_message(t('The triple store got synced with the text node content.')); 

      _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_IN_SYNC);
      return array(WISSKI_TEXTMOD_IN_SYNC, $nody->body, $nody->body);

    // do not favour a sync direction, but let the user do the merge
    // we issue a warning, though
    case WISSKI_TEXTMOD_MANUAL_SYNC:
      
      _wisski_textmod_set_sync_state($inst, $node->nid, WISSKI_TEXTMOD_NOT_IN_SYNC);
      wisski_textmod_set_sync_message($inst, $node->nid);
      return array(WISSKI_TEXTMOD_NOT_IN_SYNC, $text, $node->body);
    
    // we do nothing, we even don't warn! This can easily lead to inconsistencies but may be faster
    // can also be used to check sync status
    case WISSKI_TEXTMOD_NO_SYNC:
    default:
      
      return array(WISSKI_TEXTMOD_NOT_IN_SYNC, $text, $node->body);
  
  }

}

/**
* @author Martin Scholz
*/
function wisski_textmod_sync_instance($inst, $sync_policy = WISSKI_TEXTMOD_DEFAULT_SYNC) {
  
  $nid = wisski_store_get_nid_for_instance($inst, TRUE);
  $node = node_load($nid, NULL, TRUE); // reset cache so that we get what is in the db (inconstencies due to node_save may appear otherwise)
  
  list($sync, $itext, $ntext) = _wisski_textmod_sync($inst, $node, $sync_policy);
  return array($sync, $itext);

}


/**
* @author Martin Scholz
*/
function wisski_textmod_sync_node($node, $sync_policy = WISSKI_TEXTMOD_DEFAULT_SYNC) {
  
  $inst = wisski_store_getObj()->wisski_ARCAdapter_delNamespace($node->title);

  list($sync, $itext, $ntext) = _wisski_textmod_sync($inst, $node, $sync_policy);
  if ($sync === FALSE) return array(FALSE, $itext); // an error
  return array($sync, $ntext);

}



/**
* @author Martin Scholz
*/
function _wisski_textmod_set_sync_state($inst, $nid, $state) {
  
  $res = db_query('SELECT status FROM {wisski_textmod_sync} WHERE `nid` = %d', $nid);
  $res = db_fetch_array($res);
  if (!$res) {
    // insert
    db_query("INSERT INTO {wisski_textmod_sync} (`nid`, `instance`, `status`, `timestamp`) VALUES (%d, '%s', %d, %d)", $nid, $inst, $state, time());
  } else {
    // update; we even update if state is already set to set anew the timestamp
    db_query("UPDATE {wisski_textmod_sync} SET `status` = %d, `timestamp` = %d WHERE `nid` = %d", $state, time(), $nid);
  } 

}


/**
* @author Martin Scholz
*/
function wisski_textmod_set_sync_message($inst, $nid) {
  drupal_set_message(t('Text instance !n is not in sync. !resolve', array('!n' => l(check_plain($inst), $inst), '!resolve' => l(t('Please manually resolve.'), "node/$nid/sync_merge"))), 'warn', FALSE);
}



/**
* @author Martin Scholz
*/
function wisski_textmod_cron_sync() {
  
  module_load_include('inc', 'wisski_pathbuilder');

  $model = wisski_textmod_get_model();
  $limit = variable_get('wisski_textmod_cron_sync_limit', 500);
  $expire = variable_get('wisski_textmod_cron_sync_expire', 604800);
  $expire = time() - $expire;
  
  // mark expired instances for recheck
  db_query("UPDATE {wisski_textmod_sync} SET `status` = %d, `timestamp` = %d WHERE `status` != %d AND `status` != %d AND `timestamp` < %d", WISSKI_TEXTMOD_NEEDS_SYNC_CHECK, WISSKI_TEXTMOD_NO_TEXT_INSTANCE, time(), WISSKI_TEXTMOD_NEEDS_SYNC_CHECK, $exprire);

  // populate the agenda
  // we always load twice as many nodes as we process,
  // so that the agenda will eventually be filled with all nodes
  db_query("INSERT INTO {wisski_textmod_sync} (`nid`, `instance`, `status`, `timestamp`) SELECT `n`.`nid` , '', %d, %d FROM {node} AS n WHERE `n`.`type` = 'individual' AND `n`.`nid` NOT IN (SELECT `s`.`nid` FROM {wisski_textmod_sync} AS s WHERE 1)", WISSKI_TEXTMOD_NEEDS_SYNC_CHECK, time());

  // query $limit number of nodes and check them for sync
  // note that we check all instance nodes, regardless of whether they are text instances
  $res = db_query_range('SELECT nid FROM {wisski_textmod_sync} WHERE `status` = %d ORDER BY `timestamp` ASC', WISSKI_TEXTMOD_NEEDS_SYNC_CHECK, 0, $limit);
  while ($row = db_fetch_array($res)) {

    $nid = $row['nid'];
    $node = node_load($nid, NULL, TRUE);
    if ($node) { // node may be deleted, so check
      // check if instance is a text instance
      $inst = wisski_store_getObj()->wisski_ARCAdapter_delNamespace($node->title);
      $gid = wisski_pathbuilder_getGroupIDForIndividual($inst);
      if (!isset($model['groups'][$gid]) || empty($model['groups'][$gid]['text_class'])) {
        // we mark this instance, so that next cron runs it won't be checked again
        // will be checked again only if table gets cleared
        _wisski_textmod_set_sync_state($inst, $nid, WISSKI_TEXTMOD_NO_TEXT_INSTANCE);
      } else {
        // this is a text instance and must be checks for sync
        _wisski_textmod_sync($inst, $node, WISSKI_TEXTMOD_DEFAULT_SYNC);
      }
    } else {  // node was deleted
      db_query('DELETE FROM {wisski_textmod_sync} WHERE `nid` = %d', $nid);
    } 
  }
}

      


  

