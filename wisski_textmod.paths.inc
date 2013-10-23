<?php



/** Returns all pathbuilder paths with type description.
* Returns an array where each entry contains the path id as key
* and the value is an array containing the following information
* about the path:
* 'groups' => array of all (super)groups this path belongs to
*   the top group being the last item
* 'text_class' => the class URI of the instance that represents the text,
*   this is (disamb - 1) if set, x0 otherwise
* 'text_class_x' => the x offset in the path array, see 'text_class'
*
* The resulting array can be filtered to contain only paths that belong to
* one of $groups and/or where 'text_class' is in $text_classes.
*
* @author Martin Scholz 
*/
function wisski_textmod_get_description_paths() {

  static $paths = NULL;
  if (!empty($groups) && !is_array($groups)) $groups = array($groups);
  if (!empty($text_classes) && !is_array($text_classes)) $text_classes = array($$text_classes);

  if ($paths === NULL) {
    // on first call, we collect all desciption paths
    
    $paths = array();

    module_load_include('inc', 'wisski_pathbuilder');
    
    // consider all paths that have "description" as fieldtype
    $res = db_query('SELECT * FROM {wisski_template} WHERE `fieldtype` = "description" AND NOT `enabled` = 0 ORDER BY `weight` ASC');

    while ($path = db_fetch_array($res)) {
      $pid = $path['id'];

      $paths[$pid] = array('groups' => array(), 'text_class' => NULL, 'text_class_x' => NULL, 'pid' => $pid);

      // determine the class that holds the text
      // this is the class that the disambiguation points to
      // if no disambiguation is given, the first
      // class is taken to hold the text
      $path_array = unserialize($path['path_array']);
      $text_class_x = max(0, $path['disamb'] - 1);
      $paths[$pid]['text_class_x'] = $text_class_x;
      $paths[$pid]['text_class'] = $path_array['x' . $text_class_x];

      // collect groups (also subgroups) this path belongs to
      $gpath = $path;
      while ($gpath['group_id'] != 0) {
        $gpath = wisski_pathbuilder_getPathData($gpath['group_id']);
        $paths[$pid]['groups'][] = $gpath['id'];
      }

    }

  }

  return $paths;

}



/**
* @author Martin Scholz
*/
function wisski_textmod_get_own_texts($inst, $pid = NULL) {
  $model = wisski_textmod_get_model();

  // determine the group this individual belongs to
  // and get all paths representing text
  if ($pid === NULL) {
    $gid = wisski_pathbuilder_getGroupIDForIndividual($inst);
    if ($gid == -1) return array(TRUE, array());

    $class = $model['groups'][$gid]['text_class'];
    if ($class === NULL) {
      // group is no text group
      return array(TRUE, array());
    }
  
    //use the canonical path pattern
    $pid = $model['text_classes'][$class]['canonical_path'];

  }

  $pid_info = $model['paths'][$pid];
  $pid_x = $pid_info['text_class_x'];

  // query each path
  $q = wisski_pathbuilder_get_sparql($pid, "FILTER (?x$pid_x = <$inst>)", FALSE, $pid_x);
  $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
  $rows = $store->query($q, 'rows');
  if ($store->getErrors()) return array(FALSE, $store->getErrors());
    
  if (empty($rows)) return array(TRUE, array());

  $texts = array();
  foreach ($rows as $row) {
    $texts[] = array(
        'pid' => $pid,
        'text_instance' => $inst,
        'text' => $row['out'],
    );
  }

  return array(TRUE, $texts);

}


/**
* @author Martin Scholz
*/
function wisski_textmod_get_foreign_texts($inst, $paths = NULL, $inst_x = 0) {
  
  $model = wisski_textmod_get_model();

  // determine the group this individual belongs to
  // and get all paths representing text
  if ($paths === NULL) {
    $gid = wisski_pathbuilder_getGroupIDForIndividual($inst);
    if ($gid == -1) return array(TRUE, array());
  
    //use the foreign path patterns
    $paths = $model['groups'][$gid]['foreign_paths'];

  }

  if (empty($paths)) return array(TRUE, array());

  $texts = array();
  foreach ($paths as $pid) {

    $path = wisski_pathbuilder_getPathData($pid);
    $pid_info = $model['paths'][$pid];
    $pid_x = $pid_info['text_class_x'];

    // query each path
    $q = wisski_pathbuilder_get_sparql($pid, "FILTER (?x0 = <$inst>)", FALSE, $inst_x);
    $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
    $rows = $store->query($q, 'rows');
    if ($store->getErrors()) return array(FALSE, $store->getErrors());
  
    foreach ($rows as $row) {
      $texts[] = array(
        'pid' => $pid,
        'text_instance' => $row['x' . $pid_info['text_class_x']],
        'text' => $row['out'],
      );
    }

  }

  return array(TRUE, $texts);

}


/**Stores a text in the triple store.
* This function does not synchronize the text instance!
*
*/
function _wisski_textmod_set_own_text($text_inst, $text, $pid = NULL) {
  $text = (string) $text;

  $model = wisski_textmod_get_model();
    
  if (empty($pid)) {
    // determine the group this individual belongs to
    // and get all paths representing text
    if ($gid === NULL) {
      $gid = wisski_pathbuilder_getGroupIDForIndividual($text_inst);
      if ($gid == -1) return FALSE;
    }
   
    $class = $model['groups'][$gid]['text_class'];
    if ($class === NULL) {
      // group is no text group
      return FALSE;
    }
    
    //use the canonical path pattern
    $pid = $model['text_classes'][$class]['canonical_path'];
  }

  $pid_info = $model['paths'][$pid];
  $pid_x = $pid_info['text_class_x'];
  
//  $text_quoted = str_replace(str_split("'\n\r\\"), str_split("\\'\\n\\r\\\\", 2), $text);
  $text_quoted = $text;
  $samepart = ($pid_x < 1) ? array() : array_fill(0, $pid_x * 2, "");  // this is a dummy array
  $samepart["x$pid_x"] = $text_inst;
  $triples = _wisski_pathbuilder_generate_triples_of_path(NULL, $text_quoted, $pid, $text_inst, $samepart);
  $triples = array_reverse($triples);
  if (!$text_inst) $text_inst = $triples[0]['s'];
  
  // create first the nodes, otherwise the triples will be wiped by sync
  foreach($triples as $triple)
    wisski_store_addNodeForTriple($triple);

  $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
  $rows = $store->insert($triples, wisski_store_getObj()->wisski_ARCAdapter_graphName());
  if ($store->getErrors()) foreach ($store->getErrors() as $e) drupal_set_message("Set own Text: $e", 'error');
  
  return $text_inst;

}


/**
* @author Martin Scholz
*/
function _wisski_textmod_delete_own_text($text_inst, $text = NULL, $pid = NULL) {
  
  $model = wisski_textmod_get_model();
 
  // determine the group this individual belongs to
  // and get all paths representing text
  if ($pid === NULL) {

    $gid = wisski_pathbuilder_getGroupIDForIndividual($text_inst);
    if ($gid == -1) return FALSE;
  
    $class = $model['groups'][$gid]['text_class'];
    if ($class === NULL) {
      // group is no text group
      return FALSE;
    }
    //use the canonical path pattern
    $pid = $model['text_classes'][$class]['canonical_path'];
 
  }
  
  $path = wisski_pathbuilder_getPathData($pid);
  $pid_info = $model['paths'][$pid];
  $pid_x = $pid_info['text_class_x'];

  // query each path
  $filters = "FILTER (?x$pid_x = <$text_inst>)";
  $q = wisski_pathbuilder_get_sparql($pid, $filters, FALSE, $pid_x);
  $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
  $rows = $store->query($q,  'rows');
  if ($store->getErrors()) {
    foreach ($store->getErrors() as $e) drupal_set_message($e, 'error');
    return FALSE;
  }
  if (empty($rows)) return FALSE;

  foreach ($rows as $row) {
    if ($text === NULL || $row['out'] == $text) {
      $text_quoted = str_replace(array("\\", "'", "\n", "\r"), array("\\\\", "\\'", "\\n", "\\r"), $text);
      $i = $pid_x;
      while (isset($row["x$i"])) $i++;
      $i--;
      $q = 'DELETE { <' . $row["x$i"] . '> <' . $path['datatype_property'] . '> ?out } WHERE { <' . $row["x$i"] . '> <' . $path['datatype_property'] . "> ?out . filter(str(?out) = '$text_quoted') }";
      $store->query($q);
      if ($store->getErrors()) {
        foreach ($store->getErrors() as $e) drupal_set_message($e, 'error');
        return FALSE;
      }
    }
  }

  return TRUE;

}


/**Stores a text in the triple store.
* This function does not synchronize the text instance!
* @author Martin Scholz
*/
function _wisski_textmod_link_to_foreign_text($inst, $text_inst, $pid = NULL, $inst_x = 0) {
  $model = wisski_textmod_get_model();
  
  // determine the group this individual belongs to
  // and get first path representing text
  if ($pid === NULL) {
    $gid = wisski_pathbuilder_getGroupIDForIndividual($inst);
    if ($gid == -1) return FALSE;
    
    // use the first foreign path
    $paths = $model['groups'][$gid]['foreign_paths'];
    if (empty($paths)) return FALSE;
    $pid = $paths[0];
  }
   
  $pid_info = $model['paths'][$pid];
  $pid_x = $pid_info['text_class_x'];
  
  $samepart = ($inst_x < 1) ? array() : array_fill(0, $inst_x * 2, "");  // this is a dummy array
  $samepart['x' . $inst_x] = $inst;
  $triples = _wisski_pathbuilder_generate_triples_of_path(NULL, 'a', $pid, $inst, $samepart, $text_inst, $pid_x); // 'a' is a dummy value so that it works correctly

  $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
  $rows = $store->insert($triples, wisski_store_getObj()->wisski_ARCAdapter_graphName());
  foreach($triples as $triple)
    wisski_store_addNodeForTriple($triple);
  if (empty($inst)) $inst = $triples[0]['s'];
   
  return $inst;

}



/** Collects all the information from the pathbuilder settings
* to handle texts correctly. Also makes some integrity checks on the model.
*
* Return structure:
* array(
*   'text_classes' => array(
*     uri0 => array(
*       'class' => uri0,
*       'text_paths' => array(pid0, pid1, ...),
*       'canonical_path' => pid0,
*       'groups' => array(gid0, gid1, ...), // sorted by top group, then weight
*     ),
*     gid1 => ...
*   ),
*   'groups' => array(
      gid0 => array(
        'gid' => gid0,
        'top' => TRUE/FALSE,
        'text_class' => NULL/uri, // when not NULL, this is a text group
        'foreign_paths' => array(pid0, pid1, ...),  // pids are sorted by weight
      ),
      gid1 => ...
    ),
*   'paths' => array(
*     0 => array(
*       'pid' => pid0,
*       'groups' => array(gid0, gid1, ... gidn), // gidn is the top group
*       'text_class' => uri,
*       'text_class_x' => xn,
*     ),
*     1 => ...
*   ),
    'errors' => array(error0, error1, ...),
    'warnings' => array(warning0, warning1, ...),
* )
* @author Martin Scholz
*/
function _wisski_textmod_build_text_model_from_pb() {
  
  drupal_load('module', 'wisski_pathbuilder');
  module_load_include('inc', 'wisski_pathbuilder');
  
  $groups = array();
  $text_classes = array();
  $paths = wisski_textmod_get_description_paths();
  $errors = array();
  $warnings = array();

  // set up groups
  foreach (wisski_pathbuilder_getEnabledGroups() as $gid) {
    $data = wisski_pathbuilder_getPathData($gid);
    $data['path_array'] = _wisski_pathbuilder_calculate_group_samepart($gid);
    $groups[$gid] = array(
      'gid' => $gid,
      'top' => $data['group_id'] == 0,
      'foreign_paths' => array(), // filled later
      'text_class' => NULL, // changed later
      'data' => $data, // removed later
      'base_class' => $data['path_array']['x' . floor(count($data['path_array']) / 2)], // removed later
    );
  }
  
  // sort groups by weight, top groups first
  uasort($groups, function($a, $b) {
    if ($a['group_id'] == 0) {
      if ($b['group_id'] == 0) return $a['weight'] - $b['weight'];
      else return -1;
    }
    if ($b['group_id'] == 0) return 1;
    return $a['weight'] - $b['weight'];
  });
  

  // determine text classes and text paths
  foreach ($paths as $pid => $info) {
    $class = $info['text_class'];
    if (!isset($text_classes[$class])) {
      $text_classes[$class] = array(
        'class' => $info['text_class'],
        'text_paths' => array($pid),
        'groups' => array(), //filled later
      );

    } else {
      $text_classes[$class]['text_paths'][] = $pid;
    }
  }

  // determine groups of text classes
  foreach ($groups as $gid => $g) {
    if (isset($text_classes[$g['base_class']])) {
      $text_classes[$g['base_class']]['groups'][] = $gid;
      $groups[$gid]['text_class'] = $g['base_class'];
    }
  }

  // determine text and foreign paths
  foreach ($paths as $pid => $info) {
    $class = $info['text_class'];
    $text_classes[$class]['text_paths'][] = $pid;
    foreach ($info['groups'] as $gid) {
      if ($groups[$gid]['text_class'] != $class) {
        $groups[$gid]['foreign_paths'][] = $pid;
      }
    }
  }

  // determine canonical text path
  foreach ($text_classes as $class => $c_info) {
    $pids = $c_info['text_paths'];
    $cpid = NULL;
    $cpid_gc = NULL;
    $cpid_x = NULL;
    foreach ($pids as $pid) {
      $pid_gc = count($paths[$pid]['groups']);
      $pid_x = $paths[$pid]['text_class_x'];
      if ($cpid === NULL
         || ($pid_gc == 1 && ($cpid_gc > 1 || $pid_x < $cpid_x))
         || ($pid_gc > 1 && $cpid_gc > 1 && $pid_x < $cpid_x)) {
        $cpid = $pid;
        $cpid_gc = $pid_gc;
        $cpid_x = $pid_x;
      }
    }
    $text_classes[$class]['canonical_path'] = $cpid;
  }

  // check path defs 
  foreach ($text_classes as $c) {
    $pids = $c['text_paths'];
    $cpid = $c['canonical_path'];
    $cpid_x = $paths[$cpid]['text_class_x'];
    if (count($pids) > 1) {
      // check if classes and groups are well-defined
      foreach($pids as $pid) {
        $eq = _wisski_textmod_paths_equal($pid, $cpid, $paths[$pid]['text_class_x'], $cpid_x, -1, TRUE);
        if (!$eq) {
          $errors[] = t('Text paths with diverging definitions not supported for class %c: %pa. You want to fund this feature? Contact us!', array('%c' => $uri, '%pa' => join(', ', array_keys($pids))));
        }
      }
    }
    // check if text class is last class in path
    // From the semantics of a text instance it follows
    // that it *normally* should be the last class
    $data = wisski_pathbuilder_getPathData($cpid);
    $path_array = unserialize($data['path_array']);
    if (isset($path_array['x' . ($cpid_x + 1)])) {
      $warnings[] = t('Text class %c is not last class in path %p. From the semantics of a text instance it follows that it normally should be the last class.', array('%c' => $c, '%p' => $cpid));
    }
  }

  // check if a superclass has other text path defs => implicit diverging defs
  foreach ($text_classes as $c) {
    $pid_this = $c['canonical_path'];
    $pid_this_x = $paths[$pid_this]['text_class_x'];
    foreach(wisski_store_getObj()->wisski_ARCAdapter_getUpperClasses($c['class']) as $upper_uri) {
      $upper_uri = wisski_store_getObj()->wisski_ARCAdapter_delNamespace($upper_uri);
      if ($upper_uri == $c['class']) continue;
      if (isset($text_classes[$upper_uri])) {
        // check if canonical paths are equal
        $pid_upper = $text_classes[$upper_uri]['canonical_path'];
        $pid_upper_x = $paths[$pid_upper]['text_class_x'];
        $eq = _wisski_textmod_paths_equal($pid_upper, $pid_this, $pid_this_x, $pid_upper_x, -1, TRUE, TRUE);
        if (!$eq) $warnings[] = t('Class %c is subclass of %u but defines other text paths.', array('%c' => $c['class'], '%u' => $upper_uri));
      }
    }
  }

  // check if every text class has a top group
  foreach ($text_classes as $c) {
    $has_top = FALSE;
    foreach ($c['groups'] as $gid) {
      if ($groups[$gid]['top']) {
        $has_top = TRUE;
        break;
      }
    }
    if (!$has_top) {
      $warnings[] = t('No top group defined for text class %c.' , array('%c' => $c['class']));
    }
  }

  // remove temp items from groups
  foreach ($groups as &$g) {
    unset($g['data']);
    unset($g['base_class']);
  }
  
  $model = array(
    'paths' => $paths,
    'groups' => $groups,
    'text_classes' => $text_classes,
    'errors' => $errors,
    'warnings' => $warnings,
  );

  return $model;

}


/**
* @author Martin Scholz
*/
function _wisski_textmod_get_paths_containing($order) {
  $args = $func_get_args();
  array_shift($args);
  if (empty($args)) return FALSE;
  if (is_array($args[0])) $args = $args[0];
  
  $tmp = db_query("SELECT * FROM {wisski_template} WHERE `is_group` = 0");

  $paths = array();
  while($data = db_fetch_array($tmp)) {
    $path = unserialize($data['path_array']);
    if ($order) {
      $x = 0;
      foreach ($path as $a) {
        if ($a == $args[$x]) $x++;
        if ($x == count($args)) {
          $paths[] = $data;
          break;
        }
      }
    } else {
      if (count(array_intersect($path, $args)) == count($args)) {
        $paths[] = $data;
      }
    }
  }

  return $paths;

}



/**
* @author Martin Scholz
*/
function _wisski_textmod_paths_equal($pid1, $pid2, $begin1 = 0, $begin2 = 0, $length = -1, $incl_dtp = FALSE, $begin_with_y = FALSE) {
 
  if ($pid1 == $pid2) return $begin1 == $begin2;

  $data1 = wisski_pathbuilder_getPathData($pid1);
  if ($data1['is_group']) {
    $path1 = _wisski_pathbuilder_calculate_group_samepart($pid1);
  } else {
    $path1 = unserialize($data1['path_array']);
  }
  $data2 = wisski_pathbuilder_getPathData($pid2);
  if ($data2['is_group']) {
    $path2 = _wisski_pathbuilder_calculate_group_samepart($pid2);
  } else {
    $path2 = unserialize($data2['path_array']);
  }
  
  if ($incl_dtp) {
    if ($data1['is_group'] && $data2['is_group']) {
      // passes
    } elseif (!$data1['is_group'] && !$data2['is_group'] && $data1['datatype_property'] == $data2['datatype_property']) {
    } else {
      return FALSE; // one is group, one is path or props don't match
    }
  }

  if (!isset($path1["x$begin1"]) && !isset($path2["x$begin2"])) return TRUE;
  if (!isset($path1["x$begin1"]) || !isset($path2["x$begin2"])) return FALSE;
  if (!$begin_with_y) {
    if ($path1["x$begin1"] != $path2["x$begin2"]) return FALSE;
  }

  while ($length != 0) {
    
    if (!isset($path1["y$begin1"]) && !isset($path2["y$begin2"])) return TRUE;
    if (!isset($path1["y$begin1"]) || !isset($path2["y$begin2"])) return FALSE;
    if ($path1["y$begin1"] != $path2["y$begin2"]) return FALSE;
    
    $begin1++;
    $begin2++;
    $length--;

    if (!isset($path1["x$begin1"]) && !isset($path2["x$begin2"])) return TRUE;
    if (!isset($path1["x$begin1"]) || !isset($path2["x$begin2"])) return FALSE;
    if ($path1["x$begin1"] != $path2["x$begin2"]) return FALSE;

  }

  return TRUE;

}
