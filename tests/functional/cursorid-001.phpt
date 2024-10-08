--TEST--
Sorting single field, ascending, using the Cursor Iterator
--SKIPIF--
<?php require __DIR__ . "/../utils/basic-skipif.inc"; ?>
<?php skip_if_not_live(); ?>
<?php skip_if_not_clean(); ?>
--FILE--
<?php
require_once __DIR__ . "/../utils/basic.inc";

$manager = create_test_manager();
loadFixtures($manager);

$query = new MongoDB\Driver\Query(array(), array(
    'projection' => array('_id' => 0, 'username' => 1),
    'sort' => array('username' => 1),
    'batchSize' => 11,
    'limit' => 110,
));

$cursor = $manager->executeQuery(NS, $query);

$cursorid = $cursor->getId(true);
var_dump($cursorid);
var_dump($cursorid != 0);

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
object(MongoDB\BSON\Int64)#%d (%d) {
  ["integer"]=>
  string(%d) "%d"
}
bool(true)
===DONE===
