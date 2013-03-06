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
    _wisski_textmod_queue_for_sync($inst, $node->nid);
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
    _wisski_textmod_unqueue_for_sync($node->nid);
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

      _wisski_textmod_unqueue_for_sync($node->nid);
      return array(WISSKI_TEXTMOD_IN_SYNC, $text, $text);
    
    // the text version in the node body will be taken 
    case WISSKI_TEXTMOD_NODE_FIRST_SYNC:

      include_once(dirname(__FILE__) . '/wisski_textmod.paths.inc');
      if (!empty($text)) {
        $tmp = _wisski_textmod_delete_own_text($inst, $text, $pid);
        if (!$tmp) {
          drupal_set_message(t('Could not sync: Could not delete old text from triple store.'), 'error');
          return array(WISSKI_TEXTMOD_NOT_IN_SYNC, $text, $node->body); 
        }
      }
      if (!empty($node->body)) {
        $tmp = _wisski_textmod_set_own_text($inst, $node->body, $pid);
        if (!$tmp) {
          drupal_set_message(t('Could not sync: Could not write new text to triple store.'), 'error');
          return array(WISSKI_TEXTMOD_NOT_IN_SYNC, $text, $node->body); 
        }
      }
      drupal_set_message(t('The triple store got synced with the text node content.')); 

      _wisski_textmod_unqueue_for_sync($node->nid);
      return array(WISSKI_TEXTMOD_IN_SYNC, $nody->body, $nody->body);

    // do not favour a sync direction, but let the user do the merge
    // we issue a warning, though
    case WISSKI_TEXTMOD_MANUAL_SYNC:
      
      _wisski_textmod_queue_for_sync($inst, $node->nid);
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



function _wisski_textmod_queue_for_sync($inst, $nid) {
  
  $res = db_query('SELECT status FROM {wisski_textmod_sync} WHERE `nid` = %d', $nid);
  $res = db_fetch_array($res);
  if (!$res) {
    // insert
    db_query("INSERT INTO {wisski_textmod_sync} (`nid`, `instance`, `status`, `timestamp`) VALUES (%d, '%s', %d, '%s')", $nid, $inst, WISSKI_TEXTMOD_NOT_IN_SYNC, time());
  } elseif ($res['status'] != WISSKI_TEXTMOD_NOT_IN_SYNC) {
    // update
    db_query("UPDATE {wisski_textmod_sync} SET `status` = %d, `timestamp` = '%s' WHERE `nid` = %d", WISSKI_TEXTMOD_NOT_IN_SYNC, time(), $nid);
  } // do nothing if status is already not in sync
  drupal_set_message(t('Text instance !n is not in sync. !resolve', array('!n' => l(check_plain($inst), $inst), '!resolve' => l(t('Please manually resolve.'), "node/$nid/sync_merge"))), 'warn', FALSE);

}

function _wisski_textmod_unqueue_for_sync($nid) {
  
  db_query("DELETE FROM {wisski_textmod_sync} WHERE `nid` = %d", $nid);

}



function wisski_textmod_cron_sync() {
  
  module_load_include('inc', 'wisski_pathbuilder');
  $model = wisski_textmod_get_model();
  $limit = variable_get('wisski_textmod_cron_sync_limit', 500);

  // populate the agenda
  // we always load twice as many nodes as we process,
  // so that the agenda will eventually be filled with all nodes
  $res = db_query_range("SELECT n.nid, n.title FROM {node} AS n WHERE `n`.`type` = 'individual' AND `n`.`nid` NOT IN (SELECT `s`.`nid` FROM {wisski_textmod_sync} AS s WHERE 1)", 0, 2 * $limit);
  $values = array();
  while ($row = db_fetch_array($res)) {
    $values[] = $row['nid'];
    $values[] = wisski_store_getObj()->wisski_ARCAdapter_delNamespace($row['title']);
    $values[] = WISSKI_TEXTMOD_NEEDS_SYNC_CHECK;
    $values[] = time();

    if (count($values) % 400 == 0) {
      db_query('INSERT INTO {wisski_textmod_sync} (`nid`, `instance`, `status`, `timestamp`) VALUES ' . join(', ', array_fill(0, count($values) / 4, "(%d, '%s', %d, '%s')")), $values);
      $values = array();
    }
  }
  if (!empty($values)) {
    db_query('INSERT INTO {wisski_textmod_sync} (`nid`, `instance`, `status`, `timestamp`) VALUES ' . join(', ', array_fill(0, count($values) / 4, "(%d, '%s', %d, '%s')")), $values);
  }

  $res = db_query_range('SELECT nid, instance FROM {wisski_textmod_sync} WHERE `status` = %d ORDER BY `timestamp`', WISSKI_TEXTMOD_NEEDS_SYNC_CHECK, 0, $limit);
  
  while ($row = db_fetch_array($res)) {

    $nid = $row['nid'];
    _wisski_textmod_unqueue_for_sync($nid);
    $node = node_load($nid, NULL, TRUE);
    if ($node) { // node may be deleted, so check
      // check if instance is a text instance
      $inst = $row['instance'];
      $gid = wisski_pathbuilder_getGroupIDForIndividual($inst);
      if (!isset($model['groups'][$gid]) || empty($model['groups'][$gid]['text_class'])) continue;

      _wisski_textmod_sync($inst, $node, WISSKI_TEXTMOD_DEFAULT_SYNC);
    }
  }
}

      


  

