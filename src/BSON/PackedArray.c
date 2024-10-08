/*
 * Copyright 2014-present MongoDB, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <php.h>
#include <ext/standard/base64.h>
#include <Zend/zend_interfaces.h>
#include <Zend/zend_exceptions.h>
#include <Zend/zend_operators.h>
#include <ext/standard/php_var.h>
#include <zend_smart_str.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php_phongo.h"
#include "phongo_error.h"
#include "phongo_bson_encode.h"
#include "BSON/PackedArray_arginfo.h"
#include "BSON/Iterator.h"

zend_class_entry* php_phongo_packedarray_ce;

/* Initialize the object from a HashTable and return whether it was successful.
 * An exception will be thrown on error. */
static bool php_phongo_packedarray_init_from_hash(php_phongo_packedarray_t* intern, HashTable* props)
{
	zval* data;

	if ((data = zend_hash_str_find(props, "data", sizeof("data") - 1)) && Z_TYPE_P(data) == IS_STRING) {
		zend_string* decoded = php_base64_decode_str(Z_STR_P(data));

		intern->bson = bson_new_from_data((const uint8_t*) ZSTR_VAL(decoded), ZSTR_LEN(decoded));
		zend_string_free(decoded);

		if (intern->bson == NULL) {
			phongo_throw_exception(PHONGO_ERROR_INVALID_ARGUMENT, "%s initialization requires valid BSON", ZSTR_VAL(php_phongo_packedarray_ce->name));
			return false;
		}

		return true;
	}

	phongo_throw_exception(PHONGO_ERROR_INVALID_ARGUMENT, "%s initialization requires \"data\" string field", ZSTR_VAL(php_phongo_packedarray_ce->name));
	return false;
}

static HashTable* php_phongo_packedarray_get_properties_hash(zend_object* object, bool is_temp, int size)
{
	php_phongo_packedarray_t* intern;
	HashTable*                props;

	intern = Z_OBJ_PACKEDARRAY(object);

	PHONGO_GET_PROPERTY_HASH_INIT_PROPS(is_temp, intern, props, size);

	if (!intern->bson) {
		return props;
	}

	{
		zval data;

		ZVAL_STR(&data, php_base64_encode((const unsigned char*) bson_get_data(intern->bson), intern->bson->len));
		zend_hash_str_update(props, "data", sizeof("data") - 1, &data);
	}

	return props;
}

static bool php_phongo_packedarray_to_json(zval* return_value, bson_json_mode_t mode, const bson_t* bson)
{
	char*             json = NULL;
	size_t            json_len;
	bson_json_opts_t* opts = bson_json_opts_new(mode, BSON_MAX_LEN_UNLIMITED);
	bool              ret  = false;

	bson_json_opts_set_outermost_array(opts, true);

	json = bson_as_json_with_opts(bson, &json_len, opts);

	if (json) {
		ZVAL_STRINGL(return_value, json, json_len);
		bson_free(json);
		ret = true;
	} else {
		ZVAL_UNDEF(return_value);
		phongo_throw_exception(PHONGO_ERROR_UNEXPECTED_VALUE, "Could not convert BSON array to a JSON string");
	}

	bson_json_opts_destroy(opts);

	return ret;
}

PHONGO_DISABLED_CONSTRUCTOR(MongoDB_BSON_PackedArray)

static PHP_METHOD(MongoDB_BSON_PackedArray, fromJSON)
{
	zval                      zv;
	php_phongo_packedarray_t* intern;
	zend_string*              json;
	bson_t*                   bson;
	bson_error_t              error;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_STR(json)
	PHONGO_PARSE_PARAMETERS_END();

	bson = bson_new_from_json((const uint8_t*) ZSTR_VAL(json), ZSTR_LEN(json), &error);
	if (!bson) {
		phongo_throw_exception(PHONGO_ERROR_UNEXPECTED_VALUE, "%s", error.domain == BSON_ERROR_JSON ? error.message : "Error parsing JSON");
		return;
	}

	// Check if BSON contains only numeric keys
	if (!bson_empty(bson)) {
		bson_iter_t iter;
		uint32_t    expected_key = 0;
		char        expected_key_str[11];
		const char* key_str;

		if (!bson_iter_init(&iter, bson)) {
			phongo_throw_exception(PHONGO_ERROR_UNEXPECTED_VALUE, "Received invalid JSON array");
			bson_destroy(bson);
			return;
		}

		while (bson_iter_next(&iter)) {
			key_str = bson_iter_key(&iter);
			snprintf(expected_key_str, sizeof(expected_key_str), "%" PRIu32, expected_key);

			if (strcmp(key_str, expected_key_str)) {
				phongo_throw_exception(PHONGO_ERROR_UNEXPECTED_VALUE, "Received invalid JSON array: expected key %" PRIu32 ", but found \"%s\"", expected_key, key_str);
				bson_destroy(bson);
				return;
			}

			expected_key++;
		}
	}

	object_init_ex(&zv, php_phongo_packedarray_ce);
	intern       = Z_PACKEDARRAY_OBJ_P(&zv);
	intern->bson = bson;

	RETURN_ZVAL(&zv, 1, 1);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, fromPHP)
{
	zval                      zv;
	php_phongo_packedarray_t* intern;
	zval*                     data;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_ARRAY(data)
	PHONGO_PARSE_PARAMETERS_END();

	if (!zend_array_is_list(Z_ARRVAL_P(data))) {
		phongo_throw_exception(PHONGO_ERROR_INVALID_ARGUMENT, "Expected value to be a list, but given array is not");
		return;
	}

	object_init_ex(&zv, php_phongo_packedarray_ce);
	intern = Z_PACKEDARRAY_OBJ_P(&zv);

	intern->bson = bson_new();
	php_phongo_zval_to_bson(data, PHONGO_BSON_NONE, intern->bson, NULL);

	RETURN_ZVAL(&zv, 1, 1);
}

static bool seek_iter_to_index(bson_iter_t* iter, zend_long index)
{
	for (zend_long i = 0; i <= index; i++) {
		if (!bson_iter_next(iter)) {
			return false;
		}
	}

	return true;
}

static bool php_phongo_packedarray_get(php_phongo_packedarray_t* intern, zend_long index, zval* return_value, bool null_if_missing)
{
	bson_iter_t iter;

	if (!bson_iter_init(&iter, intern->bson)) {
		phongo_throw_exception(PHONGO_ERROR_RUNTIME, "Could not initialize BSON iterator");
		return false;
	}

	if (!seek_iter_to_index(&iter, index)) {
		if (null_if_missing) {
			ZVAL_NULL(return_value);
			return true;
		}

		phongo_throw_exception(PHONGO_ERROR_RUNTIME, "Could not find index \"%d\" in BSON array", index);
		return false;
	}

	phongo_bson_value_to_zval(bson_iter_value(&iter), return_value);

	return true;
}

static PHP_METHOD(MongoDB_BSON_PackedArray, get)
{
	php_phongo_packedarray_t* intern;
	zend_long                 index;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_LONG(index)
	PHONGO_PARSE_PARAMETERS_END();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	if (!php_phongo_packedarray_get(intern, index, return_value, false)) {
		// Exception already thrown
		RETURN_NULL();
	}
}

static PHP_METHOD(MongoDB_BSON_PackedArray, getIterator)
{
	PHONGO_PARSE_PARAMETERS_NONE();

	phongo_iterator_init(return_value, getThis());
}

static bool php_phongo_packedarray_has(php_phongo_packedarray_t* intern, zend_long index)
{
	bson_iter_t iter;

	if (!bson_iter_init(&iter, intern->bson)) {
		phongo_throw_exception(PHONGO_ERROR_RUNTIME, "Could not initialize BSON iterator");
		return false;
	}

	return seek_iter_to_index(&iter, index);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, has)
{
	php_phongo_packedarray_t* intern;
	zend_long                 index;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_LONG(index)
	PHONGO_PARSE_PARAMETERS_END();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	RETURN_BOOL(php_phongo_packedarray_has(intern, index));
}

static PHP_METHOD(MongoDB_BSON_PackedArray, toCanonicalExtendedJSON)
{
	php_phongo_packedarray_t* intern;

	PHONGO_PARSE_PARAMETERS_NONE();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	php_phongo_packedarray_to_json(return_value, BSON_JSON_MODE_CANONICAL, intern->bson);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, toRelaxedExtendedJSON)
{
	php_phongo_packedarray_t* intern;

	PHONGO_PARSE_PARAMETERS_NONE();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	php_phongo_packedarray_to_json(return_value, BSON_JSON_MODE_RELAXED, intern->bson);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, toPHP)
{
	php_phongo_packedarray_t* intern;
	zval*                     typemap = NULL;
	php_phongo_bson_state     state;

	PHONGO_PARSE_PARAMETERS_START(0, 1)
	Z_PARAM_OPTIONAL
	Z_PARAM_ARRAY(typemap)
	PHONGO_PARSE_PARAMETERS_END();

	PHONGO_BSON_INIT_STATE(state);

	if (!php_phongo_bson_typemap_to_state(typemap, &state.map)) {
		return;
	}

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	state.is_visiting_array   = true;
	state.map.int64_as_object = true;

	if (!php_phongo_bson_to_zval_ex(intern->bson, &state)) {
		zval_ptr_dtor(&state.zchild);
		php_phongo_bson_typemap_dtor(&state.map);
		RETURN_NULL();
	}

	php_phongo_bson_typemap_dtor(&state.map);

	RETURN_ZVAL(&state.zchild, 0, 1);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, offsetExists)
{
	php_phongo_packedarray_t* intern;
	zval*                     key;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_ZVAL(key)
	PHONGO_PARSE_PARAMETERS_END();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	if (Z_TYPE_P(key) != IS_LONG) {
		RETURN_FALSE;
	}

	RETURN_BOOL(php_phongo_packedarray_has(intern, Z_LVAL_P(key)));
}

static PHP_METHOD(MongoDB_BSON_PackedArray, offsetGet)
{
	php_phongo_packedarray_t* intern;
	zval*                     key;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_ZVAL(key)
	PHONGO_PARSE_PARAMETERS_END();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	if (Z_TYPE_P(key) != IS_LONG) {
		phongo_throw_exception(PHONGO_ERROR_RUNTIME, "Could not find index of type \"%s\" in BSON array", zend_zval_type_name(key));
		return;
	}

	// May throw, in which case we do nothing
	php_phongo_packedarray_get(intern, Z_LVAL_P(key), return_value, false);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, offsetSet)
{
	phongo_throw_exception(PHONGO_ERROR_LOGIC, "Cannot write to %s offset", ZSTR_VAL(php_phongo_packedarray_ce->name));
}

static PHP_METHOD(MongoDB_BSON_PackedArray, offsetUnset)
{
	phongo_throw_exception(PHONGO_ERROR_LOGIC, "Cannot unset %s offset", ZSTR_VAL(php_phongo_packedarray_ce->name));
}

static PHP_METHOD(MongoDB_BSON_PackedArray, __toString)
{
	php_phongo_packedarray_t* intern;

	PHONGO_PARSE_PARAMETERS_NONE();

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	RETVAL_STRINGL((const char*) bson_get_data(intern->bson), intern->bson->len);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, __set_state)
{
	php_phongo_packedarray_t* intern;
	HashTable*                props;
	zval*                     array;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_ARRAY(array)
	PHONGO_PARSE_PARAMETERS_END();

	object_init_ex(return_value, php_phongo_packedarray_ce);

	intern = Z_PACKEDARRAY_OBJ_P(return_value);
	props  = Z_ARRVAL_P(array);

	php_phongo_packedarray_init_from_hash(intern, props);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, serialize)
{
	php_phongo_packedarray_t* intern;
	zval                      retval;
	php_serialize_data_t      var_hash;
	smart_str                 buf = { 0 };
	zend_string*              str;

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	PHONGO_PARSE_PARAMETERS_NONE();

	array_init_size(&retval, 1);
	str = php_base64_encode(bson_get_data(intern->bson), intern->bson->len);
	ADD_ASSOC_STR(&retval, "data", str);

	PHP_VAR_SERIALIZE_INIT(var_hash);
	php_var_serialize(&buf, &retval, &var_hash);
	smart_str_0(&buf);
	PHP_VAR_SERIALIZE_DESTROY(var_hash);

	PHONGO_RETVAL_SMART_STR(buf);

	zend_string_free(str);
	smart_str_free(&buf);
	zval_ptr_dtor(&retval);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, unserialize)
{
	php_phongo_packedarray_t* intern;
	char*                     serialized;
	size_t                    serialized_len;
	zval                      props;
	php_unserialize_data_t    var_hash;

	intern = Z_PACKEDARRAY_OBJ_P(getThis());

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_STRING(serialized, serialized_len)
	PHONGO_PARSE_PARAMETERS_END();

	PHP_VAR_UNSERIALIZE_INIT(var_hash);
	if (!php_var_unserialize(&props, (const unsigned char**) &serialized, (unsigned char*) serialized + serialized_len, &var_hash)) {
		zval_ptr_dtor(&props);
		phongo_throw_exception(PHONGO_ERROR_UNEXPECTED_VALUE, "%s unserialization failed", ZSTR_VAL(php_phongo_packedarray_ce->name));

		PHP_VAR_UNSERIALIZE_DESTROY(var_hash);
		return;
	}
	PHP_VAR_UNSERIALIZE_DESTROY(var_hash);

	php_phongo_packedarray_init_from_hash(intern, HASH_OF(&props));
	zval_ptr_dtor(&props);
}

static PHP_METHOD(MongoDB_BSON_PackedArray, __serialize)
{
	PHONGO_PARSE_PARAMETERS_NONE();

	RETURN_ARR(php_phongo_packedarray_get_properties_hash(Z_OBJ_P(getThis()), true, 1));
}

static PHP_METHOD(MongoDB_BSON_PackedArray, __unserialize)
{
	zval* data;

	PHONGO_PARSE_PARAMETERS_START(1, 1)
	Z_PARAM_ARRAY(data)
	PHONGO_PARSE_PARAMETERS_END();

	php_phongo_packedarray_init_from_hash(Z_PACKEDARRAY_OBJ_P(getThis()), Z_ARRVAL_P(data));
}

/* MongoDB\BSON\PackedArray object handlers */
static zend_object_handlers php_phongo_handler_packedarray;

static void php_phongo_packedarray_free_object(zend_object* object)
{
	php_phongo_packedarray_t* intern = Z_OBJ_PACKEDARRAY(object);

	zend_object_std_dtor(&intern->std);

	if (intern->bson) {
		bson_destroy(intern->bson);
	}

	if (intern->properties) {
		zend_hash_destroy(intern->properties);
		FREE_HASHTABLE(intern->properties);
	}
}

static zend_object* php_phongo_packedarray_create_object(zend_class_entry* class_type)
{
	php_phongo_packedarray_t* intern = zend_object_alloc(sizeof(php_phongo_packedarray_t), class_type);

	zend_object_std_init(&intern->std, class_type);
	object_properties_init(&intern->std, class_type);

	intern->std.handlers = &php_phongo_handler_packedarray;

	return &intern->std;
}

static zend_object* php_phongo_packedarray_clone_object(zend_object* object)
{
	php_phongo_packedarray_t* intern;
	php_phongo_packedarray_t* new_intern;
	zend_object*              new_object;

	intern     = Z_OBJ_PACKEDARRAY(object);
	new_object = php_phongo_packedarray_create_object(object->ce);

	new_intern = Z_OBJ_PACKEDARRAY(new_object);
	zend_objects_clone_members(&new_intern->std, &intern->std);

	new_intern->bson = bson_copy(intern->bson);

	return new_object;
}

static int php_phongo_packedarray_compare_objects(zval* o1, zval* o2)
{
	php_phongo_packedarray_t *intern1, *intern2;

	ZEND_COMPARE_OBJECTS_FALLBACK(o1, o2);

	intern1 = Z_PACKEDARRAY_OBJ_P(o1);
	intern2 = Z_PACKEDARRAY_OBJ_P(o2);

	return bson_compare(intern1->bson, intern2->bson);
}

static HashTable* php_phongo_packedarray_get_debug_info(zend_object* object, int* is_temp)
{
	php_phongo_packedarray_t* intern;
	HashTable*                props;

	*is_temp = 1;
	intern   = Z_OBJ_PACKEDARRAY(object);

	/* This get_debug_info handler reports an additional property. This does not
	 * conflict with other uses of php_phongo_document_get_properties_hash since
	 * we always allocated a new HashTable with is_temp=true. */
	props = php_phongo_packedarray_get_properties_hash(object, true, 2);

	{
		php_phongo_bson_state state;

		PHONGO_BSON_INIT_STATE(state);
		state.is_visiting_array = true;
		state.map.array.type    = PHONGO_TYPEMAP_BSON;
		state.map.document.type = PHONGO_TYPEMAP_BSON;
		if (!php_phongo_bson_to_zval_ex(intern->bson, &state)) {
			zval_ptr_dtor(&state.zchild);
			goto failure;
		}

		zend_hash_str_update(props, "value", sizeof("value") - 1, &state.zchild);
	}

	return props;

failure:
	PHONGO_GET_PROPERTY_HASH_FREE_PROPS(is_temp, props);
	return NULL;
}

static HashTable* php_phongo_packedarray_get_properties(zend_object* object)
{
	return php_phongo_packedarray_get_properties_hash(object, false, 1);
}

zval* php_phongo_packedarray_read_dimension(zend_object* object, zval* offset, int type, zval* rv)
{
	php_phongo_packedarray_t* intern;

	intern = Z_OBJ_PACKEDARRAY(object);

	if (Z_TYPE_P(offset) != IS_LONG) {
		if (type == BP_VAR_IS) {
			ZVAL_NULL(rv);
			return rv;
		}

		phongo_throw_exception(PHONGO_ERROR_RUNTIME, "Could not find index of type \"%s\" in BSON array", zend_zval_type_name(offset));
		return &EG(uninitialized_zval);
	}

	if (!php_phongo_packedarray_get(intern, Z_LVAL_P(offset), rv, type == BP_VAR_IS)) {
		// Exception already thrown
		return &EG(uninitialized_zval);
	}

	return rv;
}

void php_phongo_packedarray_write_dimension(zend_object* object, zval* offset, zval* value)
{
	phongo_throw_exception(PHONGO_ERROR_LOGIC, "Cannot write to %s offset", ZSTR_VAL(php_phongo_packedarray_ce->name));
}

int php_phongo_packedarray_has_dimension(zend_object* object, zval* member, int check_empty)
{
	php_phongo_packedarray_t* intern;

	intern = Z_OBJ_PACKEDARRAY(object);

	if (Z_TYPE_P(member) != IS_LONG) {
		return false;
	}

	return php_phongo_packedarray_has(intern, Z_LVAL_P(member));
}

void php_phongo_packedarray_unset_dimension(zend_object* object, zval* offset)
{
	phongo_throw_exception(PHONGO_ERROR_LOGIC, "Cannot unset %s offset", ZSTR_VAL(php_phongo_packedarray_ce->name));
}

void php_phongo_packedarray_init_ce(INIT_FUNC_ARGS)
{
	php_phongo_packedarray_ce                = register_class_MongoDB_BSON_PackedArray(zend_ce_aggregate, zend_ce_serializable, zend_ce_arrayaccess, php_phongo_type_ce, zend_ce_stringable);
	php_phongo_packedarray_ce->create_object = php_phongo_packedarray_create_object;

	memcpy(&php_phongo_handler_packedarray, phongo_get_std_object_handlers(), sizeof(zend_object_handlers));
	php_phongo_handler_packedarray.compare         = php_phongo_packedarray_compare_objects;
	php_phongo_handler_packedarray.clone_obj       = php_phongo_packedarray_clone_object;
	php_phongo_handler_packedarray.get_debug_info  = php_phongo_packedarray_get_debug_info;
	php_phongo_handler_packedarray.get_properties  = php_phongo_packedarray_get_properties;
	php_phongo_handler_packedarray.free_obj        = php_phongo_packedarray_free_object;
	php_phongo_handler_packedarray.read_dimension  = php_phongo_packedarray_read_dimension;
	php_phongo_handler_packedarray.write_dimension = php_phongo_packedarray_write_dimension;
	php_phongo_handler_packedarray.has_dimension   = php_phongo_packedarray_has_dimension;
	php_phongo_handler_packedarray.unset_dimension = php_phongo_packedarray_unset_dimension;
	php_phongo_handler_packedarray.offset          = XtOffsetOf(php_phongo_packedarray_t, std);
}

bool phongo_packedarray_new(zval* object, bson_t* bson, bool copy)
{
	php_phongo_packedarray_t* intern;

	object_init_ex(object, php_phongo_packedarray_ce);

	intern       = Z_PACKEDARRAY_OBJ_P(object);
	intern->bson = copy ? bson_copy(bson) : bson;

	return true;
}
