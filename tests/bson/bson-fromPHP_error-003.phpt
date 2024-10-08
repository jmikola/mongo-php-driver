--TEST--
MongoDB\BSON\fromPHP(): Encoding non-Serializable Type objects as a root element
--FILE--
<?php

require_once __DIR__ . '/../utils/basic.inc';

class UnknownType implements MongoDB\BSON\Type {}

$tests = array(
    new UnknownType,
    new MongoDB\BSON\Binary('foobar', MongoDB\BSON\Binary::TYPE_GENERIC),
    new MongoDB\BSON\Javascript('function(bar) {var baz = bar; var bar = foo; return bar; }'),
    new MongoDB\BSON\MinKey,
    new MongoDB\BSON\MaxKey,
    new MongoDB\BSON\ObjectId,
    new MongoDB\BSON\Regex('regexp', 'i'),
    new MongoDB\BSON\Timestamp(1234, 5678),
    new MongoDB\BSON\UTCDateTime(new MongoDB\BSON\Int64('1416445411987')),
);

foreach ($tests as $document) {
    echo throws(function() use ($document) {
        MongoDB\BSON\fromPHP($document);
    }, 'MongoDB\Driver\Exception\UnexpectedValueException'), "\n";}

?>
===DONE===
<?php exit(0); ?>
--EXPECTF--
Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance UnknownType cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\Binary cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\Javascript cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\MinKey cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\MaxKey cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\ObjectId cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\Regex cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\Timestamp cannot be serialized as a root element

Deprecated: Function MongoDB\BSON\fromPHP() is deprecated in %s
OK: Got MongoDB\Driver\Exception\UnexpectedValueException
MongoDB\BSON\Type instance MongoDB\BSON\UTCDateTime cannot be serialized as a root element
===DONE===
