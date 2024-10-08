--TEST--
MongoDB\BSON\UTCDateTime #001
--SKIPIF--
<?php require __DIR__ . "/../utils/basic-skipif.inc"; ?>
<?php skip_if_not_live(); ?>
<?php skip_if_not_clean(); ?>
--FILE--
<?php

require_once __DIR__ . "/../utils/basic.inc";

$manager = create_test_manager();

$utcdatetime = new MongoDB\BSON\UTCDateTime(new MongoDB\BSON\Int64('1416445411987'));

$bulk = new MongoDB\Driver\BulkWrite();
$bulk->insert(array('_id' => 1, 'x' => $utcdatetime));
$manager->executeBulkWrite(NS, $bulk);

$query = new MongoDB\Driver\Query(array('_id' => 1));
$cursor = $manager->executeQuery(NS, $query);
$results = iterator_to_array($cursor);

$tests = array(
    array($utcdatetime),
    array($results[0]->x),
);

foreach($tests as $n => $test) {
    $s = fromPHP($test);
    echo "Test#{$n} ", $json = toJSON($s), "\n";
    $bson = fromJSON($json);
    $testagain = toPHP($bson);
    var_dump(toJSON(fromPHP($test)), toJSON(fromPHP($testagain)));
    var_dump((object)$test == (object)$testagain);
}
?>
===DONE===
<?php exit(0); ?>
--EXPECT--
Test#0 { "0" : { "$date" : { "$numberLong" : "1416445411987" } } }
string(59) "{ "0" : { "$date" : { "$numberLong" : "1416445411987" } } }"
string(59) "{ "0" : { "$date" : { "$numberLong" : "1416445411987" } } }"
bool(true)
Test#1 { "0" : { "$date" : { "$numberLong" : "1416445411987" } } }
string(59) "{ "0" : { "$date" : { "$numberLong" : "1416445411987" } } }"
string(59) "{ "0" : { "$date" : { "$numberLong" : "1416445411987" } } }"
bool(true)
===DONE===
