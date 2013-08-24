/*
 * recordReader.h
 *
 *  Created on: Aug 2, 2013
 *      Author: gokhan
 */

#ifndef RECORDREADER_H_
#define RECORDREADER_H_

#include "postgres.h"
#include "nodes/primnodes.h"

#include "orc.pb-c.h"
#include "inputStream.h"

#define DATA_STREAM 			0
#define LENGTH_STREAM 			1
#define SECONDARY_STREAM 		1
#define DICTIONARY_DATA_STREAM	2

#define MAX_STREAM_COUNT			3
#define STRING_STREAM_COUNT			3
#define STRING_DIRECT_STREAM_COUNT	2
#define TIMESTAMP_STREAM_COUNT		2
#define BINARY_STREAM_COUNT			2
#define COMMON_STREAM_COUNT			1

#define MAX_POSTSCRIPT_SIZE		255

/* timestamp related values */
#define SECONDS_PER_DAY					86400
#define MICROSECONDS_PER_SECOND			1000000L
#define NANOSECONDS_PER_MICROSECOND		1000
#define POSTGRESQL_EPOCH_IN_SECONDS		946677600L
#define ORC_EPOCH_IN_SECONDS			1420063200L
#define ORC_DIFF_POSTGRESQL				473385600L
#define ORC_PSQL_EPOCH_IN_DAYS			10957

#define ToUnsignedInteger(x) (uint64_t)( ((x) < 0) ? ( ((uint64_t)-(x+1)) * 2 + 1) : (2 * (uint64_t)(x)))
#define ToSignedInteger(x)   ( int64_t)( ((x) % 2) ? (-(int64_t)((x - 1) / 2) - 1) : ((x) / 2) )

#define IsComplexType(type) \
			  (type == FIELD_TYPE__KIND__LIST \
			|| type == FIELD_TYPE__KIND__STRUCT \
			|| type == FIELD_TYPE__KIND__MAP)

typedef enum
{
	VARIABLE_LENGTH, RLE
} EncodingType;

typedef union
{
	uint8_t value8;
	int64_t value64;
	float floatValue;
	double doubleValue;
	char* binary;
	struct timespec time;
} FieldValue;

typedef struct
{
	union
	{
		FieldValue value;
		FieldValue* list;
	};
	int* listItemSizes;
	char* isItemNull;
} Field;

typedef struct
{
	/* stream to read from the file */
	FileStream* stream;

	/* type of the encoding */
	EncodingType currentEncodingType;

	/* byte that is used currently */
	uint64_t data;

	/* no of bytes left in the current run */
	short noOfLeftItems;

	/* mask is for boolean, step is for int, float&double is for floating point readers */
	union
	{
		uint8_t mask;
		char step;
		float floatData;
		double doubleData;
	};

} StreamReader;

/**
 * Base structure to represent a field reader.
 * Its fieldreader variable contains the streams of the type.
 */
typedef struct
{
	StreamReader presentBitReader;
	FieldType__Kind kind;
	RowIndex* rowIndex;

	int orcColumnNo;

	/* Length reader for list & map complex types */
	char hasPresentBitReader;
	char required;

	/* Actual field reader, can be struct, list, or primitive */
	void* fieldReader;

//	/* psql column information */
//	int psqlKind;
//	int columnTypeMod;
	Var* psqlVariable;
} FieldReader;

/**
 * Reader for primitive types
 * (boolean, byte, binary, short, integer, long, float, double, string)
 */
typedef struct
{
	StreamReader readers[MAX_STREAM_COUNT];

	/* for string type to store the dictionary */
	char hasDictionary;
	int dictionarySize;
	int* wordLength;
	char** dictionary;
} PrimitiveFieldReader;

/*
 * Reader for list types.
 */
typedef struct
{
	StreamReader lengthReader;
	FieldReader itemReader;
} ListFieldReader;

typedef struct
{
	int noOfFields;
	FieldReader** fields;
} StructFieldReader;

int StreamReaderFree(StreamReader* streamReader);
int StreamReaderInit(StreamReader* streamReader, FieldType__Kind streamKind, FILE* file,
		long offset, long limit, CompressionParameters* parameters);
int StreamReaderSkip(StreamReader* streamReader, int noOfElements);

/**
 * Reads one element from the type.
 * The returned value is <0 for error, 1 for null, 0 for not-null value
 */
int FieldReaderRead(FieldReader* fieldReader, Field* field, int* length);
int ReadDictionary(FieldReader* fieldReader);
int FieldReaderFree(FieldReader* reader);

/*
 * Functions to read the column value directly into the native PSQL format
 */
Datum ReadPrimitiveFieldAsDatum(FieldReader* fieldReader, bool *isNull);
Datum ReadListFieldAsDatum(FieldReader* fieldReader, bool *isNull);

/**
 * Helper functions to get the kth stream and its type
 */
FieldType__Kind GetStreamKind(FieldType__Kind type, ColumnEncoding__Kind encoding, int streamIndex);
int GetStreamCount(FieldType__Kind type, ColumnEncoding__Kind encoding);

#endif /* RECORDREADER_H_ */
