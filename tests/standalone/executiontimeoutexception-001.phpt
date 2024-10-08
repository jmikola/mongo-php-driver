--TEST--
ExecutionTimeoutException: exceeding $maxTimeMS (queries)
--SKIPIF--
<?php require __DIR__ . "/../utils/basic-skipif.inc"; ?>
<?php skip_if_test_commands_disabled(); ?>
<?php skip_if_not_clean(); ?>
--FILE--
<?php
require_once __DIR__ . "/../utils/basic.inc";

$manager = create_test_manager();

// Select a specific server for future operations to avoid mongos switching in sharded clusters
$server = $manager->selectServer(new \MongoDB\Driver\ReadPreference('primary'));

$query = new MongoDB\Driver\Query(array("company" => "Smith, Carter and Buckridge"), array(
    'projection' => array('_id' => 0, 'username' => 1),
    'sort' => array('phoneNumber' => 1),
    'maxTimeMS' => 1,
));

failMaxTimeMS($server);
throws(function() use ($server, $query) {
    $result = $server->executeQuery(NS, $query);
}, "MongoDB\Driver\Exception\ExecutionTimeoutException");

?>
===DONE===
<?php exit(0); ?>
--EXPECT--
OK: Got MongoDB\Driver\Exception\ExecutionTimeoutException
===DONE===
