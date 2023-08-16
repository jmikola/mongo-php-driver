--TEST--
MongoDB\BSON\Regex unserialization will alphabetize flags (__serialize and __unserialize)
--FILE--
<?php

var_dump(unserialize('O:18:"MongoDB\BSON\Regex":2:{s:7:"pattern";s:6:"regexp";s:5:"flags";s:6:"xusmli";}'));

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
object(MongoDB\BSON\Regex)#%d (%d) {
  ["pattern"]=>
  string(6) "regexp"
  ["flags"]=>
  string(6) "ilmsux"
}
===DONE===
