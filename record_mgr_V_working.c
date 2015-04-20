#include "dberror.h"
#include "expr.h"
#include "tables.h"
#include "buffer_mgr.h"
#include "record_mgr.h"
#include "storage_mgr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h> 
#include <sys/types.h> 
#include <sys/stat.h> 
#include <unistd.h> 
#include <errno.h> 
#include <fcntl.h> 

#define FIELD_DELIMITER ","
#define TUPLE_DELIMITER ";"

typedef struct Multiple_Scan_Handler
{
Expr* cond;
int page;
int slot;
}Multiple_Scan_Handler;

int prevSlot = 0;int prevRecSlot = 0;int count = 0;int flag = 0;int pageNo = 1;
// table and manager
RC initRecordManager(void *mgmtData) {
	return RC_OK;
}
RC shutdownRecordManager() {
	prevSlot = 0;prevRecSlot = 0;count = 0;flag = 0;pageNo=1;
	return RC_OK;
}
char* serialize_data(char* schema_info, Schema *schema) {
	char* numberofAttributes = (char *) malloc(1);
	// Convert the integer to char* using sprintnf
	sprintf(numberofAttributes, "%d", schema->numAttr);
	// Append both the values using strcat
	strcat(schema_info, numberofAttributes);
	strcat(schema_info, "\n");
	strcat(schema_info, schema->attrNames[0]);
	strcat(schema_info, ",");
	strcat(schema_info, schema->attrNames[1]);
	strcat(schema_info, ",");
	strcat(schema_info, schema->attrNames[2]);
	strcat(schema_info, "\n");
	sprintf(numberofAttributes, "%d", schema->dataTypes[0]);
	strcat(schema_info, numberofAttributes);
	strcat(schema_info, ",");
	sprintf(numberofAttributes, "%d", schema->dataTypes[1]);
	strcat(schema_info, numberofAttributes);
	strcat(schema_info, ",");
	sprintf(numberofAttributes, "%d", schema->dataTypes[2]);
	strcat(schema_info, numberofAttributes);
	strcat(schema_info, "\n");
	sprintf(numberofAttributes, "%d", schema->typeLength[0]);
	strcat(schema_info, numberofAttributes);
	strcat(schema_info, ",");
	sprintf(numberofAttributes, "%d", schema->typeLength[1]);
	strcat(schema_info, numberofAttributes);
	strcat(schema_info, ",");
	sprintf(numberofAttributes, "%d", schema->typeLength[2]);
	strcat(schema_info, numberofAttributes);

	return schema_info;
}

RC createTable(char *name, Schema *schema) {
	printf("----START----[CREATE TABLE]------------\n");

	BM_BufferPool *bm = MAKE_POOL();
	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();

	createPageFile(name);
	initBufferPool(bm, name, 3, RS_FIFO, NULL);

	pinPage(bm, bm_ph, 0);
	markDirty(bm, bm_ph);

	printf(
			"Schema[0] values are attrNames[%s] , dataTypes[%d] , typeLength[%d] \n",
			schema->attrNames[0], schema->dataTypes[0], schema->typeLength[0]);
	printf(
			"Schema[1] values are attrNames[%s] , dataTypes[%d] , typeLength[%d] \n",
			schema->attrNames[1], schema->dataTypes[1], schema->typeLength[1]);
	printf(
			"Schema[2] values are attrNames[%s] , dataTypes[%d] , typeLength[%d] \n",
			schema->attrNames[2], schema->dataTypes[2], schema->typeLength[2]);

	char* schema_info = NULL;
	schema_info = (char *) malloc(strlen(name));
	memcpy(schema_info, name, strlen(name));
	strcat(schema_info, "\n");

	// Call the Serialize Method to convert all the data types to character pointer and store it in the first page.
	schema_info = serialize_data(schema_info, schema);

	//Copy the resulting Schema info to BM Page Handle Data.
	memcpy(bm_ph->data, schema_info, strlen(schema_info));
	printf("Data [%s] and Info [%s] \n", bm_ph->data, schema_info);
	/*	free(schema_info);
	 */unpinPage(bm, bm_ph);

	printf("----END----[CREATE TABLE]------------\n");
	return RC_OK;
}

RM_TableData* deserialize_data(RM_TableData *rel, char *data, Schema *schema) {

//	strcpy(data,"test_table_t\n3\na,b,c\n0,1,0\n0,4,0");
	int i = 0, j = 0, k = 0, l = 0;
	char *temp_buff[5];
	char *temp_buff2[3];
	DataType temp_buff3[3];
	int temp_buff4[3];
	char *buff = strtok(data, "\n");
	while (buff != NULL) {
		//printf(buff);
		temp_buff[i] = buff;
		buff = strtok(NULL, "\n");
		i++;
	}
	char *buff2 = strtok(temp_buff[2], ",");
	while (buff2 != NULL) {
		//printf(buff);
		temp_buff2[j] = buff2;
		buff2 = strtok(NULL, ",");
		j++;
	}
	char *buff3 = strtok(temp_buff[3], ",");
	while (buff3 != NULL) {
		//printf(buff);
		temp_buff3[k] = atoi(buff3);
		buff3 = strtok(NULL, ",");
		k++;
	}
	char *buff4 = strtok(temp_buff[4], ",");
	while (buff4 != NULL) {
		//printf(buff);
		temp_buff4[l] = atoi(buff4);
		buff4 = strtok(NULL, ",");
		l++;
	}

	int m;
	char **cpNames1 = (char **) malloc(sizeof(char*) * 3);
	for (m = 0; m < 3; m++) {
		cpNames1[m] = (char *) malloc(2);
		strcpy(cpNames1[m], temp_buff2[m]);
	}

	//Schema *schema;
	schema->attrNames = cpNames1;

	char *asl = temp_buff[1];
	int z = atoi(asl);
	schema->numAttr = z;

	int *cpSizes1 = (int *) malloc(sizeof(int) * 3);
	memcpy(cpSizes1, temp_buff4, sizeof(int) * 3);
	schema->typeLength = cpSizes1;

	DataType *cpDt1 = (DataType *) malloc(sizeof(DataType) * 3);
	memcpy(cpDt1, temp_buff3, sizeof(DataType) * 3);
	schema->dataTypes = cpDt1;

	schema->keySize = 0;

	rel->schema = schema;

	return rel;
}

RC openTable(RM_TableData *rel, char *name) {
	printf("----START----[OPEN TABLE]------------\n");
	BM_BufferPool *bm = MAKE_POOL();
	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();

	initBufferPool(bm, name, 3, RS_FIFO, NULL);

	pinPage(bm, bm_ph, 0);
	markDirty(bm, bm_ph);
	RM_TableData *rel1;
	Schema *schema;
	rel1 = deserialize_data(rel, bm_ph->data, schema);

	rel->schema = rel1->schema;
	rel->name = name;
	rel->mgmtData = (void *) bm;
	printf(
			"Schema[0] values are attrNames[%s] and NoAttributes[%d] and TypeLength = cpSizes[%d] and DataTypes = cpDt1[%d]\n",
			rel->schema->attrNames[0], rel->schema->numAttr,
			rel->schema->typeLength[0], rel->schema->dataTypes[0]);
	printf(
			"Schema[1] values are attrNames[%s] and NoAttributes[%d] and TypeLength = cpSizes[%d] and DataTypes = cpDt1[%d]\n",
			rel->schema->attrNames[1], rel->schema->numAttr,
			rel->schema->typeLength[1], rel->schema->dataTypes[1]);
	printf(
			"Schema[2] values are attrNames[%s] and NoAttributes[%d] and TypeLength = cpSizes[%d] and DataTypes = cpDt1[%d]\n",
			rel->schema->attrNames[2], rel->schema->numAttr,
			rel->schema->typeLength[2], rel->schema->dataTypes[2]);

	printf("----END----[OPEN TABLE]------------\n");
	return RC_OK;
}
RC closeTable(RM_TableData *rel) {
	printf("----START----[CLOSE TABLE]------------\n");
	BM_BufferPool *bm = (BM_BufferPool *) rel->mgmtData;
	/*shutdownBufferPool(bm);

	SM_FileHandle *fh;
	fh->fileName=bm->pageFile;
	closePageFile(fh);*/
	printf("----END----[CLOSE TABLE]------------\n");
	return RC_OK;
}
RC deleteTable(char *name) {
	printf("----START----[DELETE TABLE]------------\n");
	destroyPageFile(name);
	printf("----END----[DELETE TABLE]------------\n");
	return RC_OK;
}
int getNumTuples(RM_TableData *rel) {
	printf("----START----[GET NUM TUPLES]------------\n");

	printf("----END----[GET NUM TUPLES]------------\n");
	return RC_OK;
}

// handling records in a table
RC insertRecord(RM_TableData *rel, Record *record) {
	printf("----START----[INSERT RECORD]------------\n");
	BM_BufferPool *bm = (BM_BufferPool *) rel->mgmtData;
	printf("File Name is [%s] and Number [%d] and strat [%d] Data is [%s]\n",
	bm->pageFile, bm->numPages, bm->strategy, record->data+22);

	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();
	SM_FileHandle fHandle;//pageNo
	fHandle.fileName = bm->pageFile;
	pinPage(bm, bm_ph, pageNo);int i=0;
	printf("Before Append [%s]\n", bm_ph->data);
	int nullToAppend = 16 - strlen(record->data);
	if(nullToAppend >0){
		while(i<nullToAppend){
			record->data = strcat(record->data,"-");i++;
		}
	}
	bm_ph->data = strcat(bm_ph->data, record->data);
	int slotForEachRecord = strlen(bm_ph->data) - strlen(record->data) + 1;
	printf("After Append [%s]\n", bm_ph->data);
	printf("String Length = %zd and Total String Length = %zd \n",strlen(record->data),strlen(bm_ph->data));
	bm_ph->pageNum = pageNo;
	printf("Page Number Running :- %d \n",bm_ph->pageNum);

	markDirty(bm, bm_ph);
	unpinPage(bm, bm_ph);
	RID rID;
	rID.page = pageNo;
	if(slotForEachRecord == 1){
		rID.slot = slotForEachRecord;
	}
	else{
		if(strlen(bm_ph->data)%4096 == 0){
			rID.slot = strlen(bm_ph->data) - 16;
		}
		else{
			rID.slot = (strlen(bm_ph->data)%4096) - 16;
		}
		prevRecSlot = rID.slot;
	}
	record->id = rID;
	if(strlen(bm_ph->data) % 4096 == 0){
		printf("Page Full. So Appending one more page using ensure Capacity. \n");
		int stat = ensureCapacity1(pageNo+2,&fHandle);
		pageNo = pageNo + 1;
	}
	printf("Slot Number in RID and inserted in page is [%d] and [%d]\n",record->id.slot,pageNo);
	printf("----END----[INSERT RECORD]------------\n");
	return RC_OK;

}
RC deleteRecord(RM_TableData *rel, RID id) {
	printf("----START----[DELETE RECORD]------------\n");
	
	int i = 0;char *alpha=malloc(16);char *updated_rec_data = malloc(PAGE_SIZE);
	int page=id.page;
	int slot=id.slot;printf("Slot Number = %d \n",slot);
	BM_BufferPool *bm = (BM_BufferPool *) rel->mgmtData;
	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();
	pinPage(bm,bm_ph,page);

	while(i<16){
		alpha = strcat(alpha,"-");i++;			
	}
	updated_rec_data = strncat(updated_rec_data,bm_ph->data,slot);
	updated_rec_data = strcat(updated_rec_data,alpha);
	updated_rec_data = strcat(updated_rec_data,bm_ph->data+slot+16);
	
	strcpy(bm_ph->data,updated_rec_data);
	printf("Data After Deletion :- %s \n",updated_rec_data);
	markDirty(bm, bm_ph);
	unpinPage(bm, bm_ph);

	printf("----END----[DELETE RECORD]------------\n");
	return RC_OK;
}
RC updateRecord(RM_TableData *rel, Record *record) {
	printf("----START----[UPDATE RECORD]------------\n");

	printf("Page and Slot = %d and %d\n",record->id.page,record->id.slot);
	RID rid=record->id;char *rec_data;char *updated_rec_data = malloc(PAGE_SIZE);
	rec_data=record->data;
	int page=rid.page;
	int slot=rid.slot;
	if(slot == 1){
		slot = 0;
	}
	printf("Updated Record= %s \n",rec_data);
	BM_BufferPool *bm = (BM_BufferPool *) rel->mgmtData;
	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();
	pinPage(bm, bm_ph, record->id.page);
	printf("Before Append [%s]\n", bm_ph->data);
	char *alpha = malloc(16);
	strncpy(alpha,bm_ph->data+slot,16);
	printf("---Need to be Modified String = %s \n",alpha);
	printf("Updated String = %s \n",record->data);
	int nullToAppend = 16 - strlen(record->data);int i = 0;
	if(nullToAppend >0){
		while(i<nullToAppend){
			record->data = strcat(record->data,"-");i++;			
		}
	}
	printf("Dash 16 Updated String = %s \n",record->data);	
	updated_rec_data = strncat(updated_rec_data,bm_ph->data,slot);
	updated_rec_data = strcat(updated_rec_data,record->data);
	updated_rec_data = strcat(updated_rec_data,bm_ph->data+slot+16);
	printf("After Append [%s]\n", updated_rec_data);
	strcpy(bm_ph->data,updated_rec_data);
	bm_ph->pageNum = record->id.page;
	markDirty(bm, bm_ph);
	unpinPage(bm, bm_ph);
	printf("Record Page and Slot :- %d and %d \n",record->id.page,record->id.slot);
	printf("----END----[UPDATE RECORD]------------\n");
	return RC_OK;
}

RC getRecord(RM_TableData *rel, RID id, Record *record) {
	printf("----START----[GET RECORD]------------\n");
	record->id=id;
	printf("Page and Slot = %d and %d",record->id.page,record->id.slot);

	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();
//	bm_ph->pageNum = 1;
	BM_BufferPool *bm = (BM_BufferPool *) rel->mgmtData;
	pinPage(bm,bm_ph,record->id.page);//ink record->id.page to 1
	bm_ph->pageNum = record->id.page;
	markDirty(bm, bm_ph);
	unpinPage(bm, bm_ph);
	char *rec_data;
	if(record->id.slot==1){
	rec_data = malloc(sizeof(char) * 16);
	strncpy(rec_data,bm_ph->data+record->id.slot-1,16); 
	strcpy(record->data,rec_data);
	}
	else{
	rec_data = malloc(sizeof(char) * 16);
	strncpy(rec_data,bm_ph->data+record->id.slot,16);
	
	strncpy(record->data,rec_data,16);
	}
	printf("-XX--%s--and %lu--XX--\n",record->data,strlen(record->data));
	prevSlot = record->id.slot;
	printf("----END----[GET RECORD]------------\n");
	free(rec_data);
	return RC_OK;
}

// scans
RC startScan(RM_TableData *rel, RM_ScanHandle *scan, Expr *cond) {
	printf("----START----[START SCAN]------------\n");

	scan->rel = rel;
	Multiple_Scan_Handler* m_sh = (Multiple_Scan_Handler*)malloc(sizeof(Multiple_Scan_Handler));
	m_sh->page=1;m_sh->slot =0;
	m_sh->cond = (Expr *)cond;
	scan->mgmtData=(Multiple_Scan_Handler*)m_sh;

	return RC_OK;
}

RC next(RM_ScanHandle *scan, Record *record) {
	printf("----START----[NEXT]------------\n");

	RM_TableData *rel = scan->rel;

	Value *value;int i = 0;int flag = 0;
	BM_PageHandle *bm_ph = MAKE_PAGE_HANDLE();

	BM_BufferPool *bm = (BM_BufferPool *) rel->mgmtData;
	
	Multiple_Scan_Handler* m_sh = (Multiple_Scan_Handler*)scan->mgmtData;
	Expr *cond = (Expr *)m_sh->cond;
	printf("Record Page = %d and Slot = %d\n",m_sh->page,m_sh->slot);

	RID id; id.page=1;

	pinPage(bm,bm_ph,1);
	char *data = bm_ph->data;
	int totalSlots = strlen(data) / 16;
	printf("No of Slots = %d \n",totalSlots);

	for(;m_sh->slot<totalSlots;m_sh->slot++)
		{
			if(m_sh->slot == 0){id.slot = 1;}
			else{id.slot=m_sh->slot*16;}
			getRecord(rel,id,record);
			evalExpr(record, rel->schema, cond, &value);
			if(value->v.boolV)
			{
				flag=1;
				m_sh->slot++;
				break;
			}
	}
	bm_ph->pageNum = 1;
	unpinPage(bm, bm_ph);
	printf("----END----[NEXT]------------\n");
	//strcpy(record->data,";3,cccc,1;");
	
	if(flag == 1)
		return RC_OK;
	else
		return RC_RM_NO_MORE_TUPLES;
}
RC closeScan(RM_ScanHandle *scan) {
	printf("----START----[CLOSE SCAN]------------\n");

//free(scan->mgmtData);
//scan->mgmtData=NULL;

	printf("----END----[CLOSE SCAN]------------\n");
	return RC_OK;
}

// dealing with schemas
int getRecordSize(Schema *schema) {
	printf("----START----[GET RECORD SIZE]------------\n");

	int record_size = 0, i = 0;
	// based on number of attributes
	for (i = 0; i < schema->numAttr; i++) {
		// Decide on type length and add that to schema
		if (schema->typeLength[i] == 0){
			record_size = record_size + sizeof(DT_INT);}
		else{
			record_size = record_size + schema->typeLength[i];}
	}
	printf("----END----[GET RECORD SIZE]------------\n");
	return record_size - 2;

}
Schema *createSchema(int numAttr, char **attrNames, DataType *dataTypes,
		int *typeLength, int keySize, int *keys) {
	printf("----START----[CREATE SCHEMA]------------\n");
	Schema* schema = (Schema*) malloc(sizeof(Schema));

	// Assign all attributes of the schema passed
	schema->numAttr = numAttr;
	schema->attrNames = attrNames;
	schema->dataTypes = dataTypes;
	schema->typeLength = typeLength;
	schema->keyAttrs = keys;
	schema->keySize = keySize;

	printf("----END----[CREATE SCHEMA]------------\n");
	// return schema
	return schema;
}
RC freeSchema(Schema *schema) {
	printf("----START----[FREE SCHEMA]------------\n");

	printf("----END----[FREE SCHEMA]------------\n");
	return RC_OK;
}

// dealing with records and attribute values
RC createRecord(Record **record, Schema *schema) {
	printf("----START----[CREATE RECORD]------------\n");

	int rec_size;
	(*record) = (Record*) malloc(sizeof(Record));
	rec_size = getRecordSize(schema);

	(*record)->data = (char*) malloc(sizeof(char)*rec_size +4);
	char *x = ";";
	strcpy((*record)->data, x);

	printf("----END----[CREATE RECORD]------{size = %zd}------\n",sizeof((*record)->data));
	return RC_OK;
}
RC freeRecord(Record *record) {
	printf("----START----[FREE RECORD]------------\n");
	free(record->data);
	record->data=NULL;
	printf("----END----[FREE RECORD]------------\n");
	return RC_OK;
}
RC getAttr(Record *record, Schema *schema, int attrNum, Value **value) {
	printf("----START----[GET ATTR]------------\n");
	printf("Data is %s\n",record->data);
	printf("Data Types are %d and %d and %d\n",schema->dataTypes[0],schema->dataTypes[1],schema->dataTypes[2]);
	printf("Data Types Length are %d and %d and %d\n",schema->typeLength[0],schema->typeLength[1],schema->typeLength[2]);
	printf("Data = %s \n",record->data);

	char *a,*temp_buff[2];int i=0;
	char *x = malloc(15);strcpy(x,record->data);
	a = strtok(x,TUPLE_DELIMITER);
	printf("Data1 = %s \n",record->data);
	char *buff = strtok(a,FIELD_DELIMITER);
	while (buff != NULL) {
		temp_buff[i] = buff;
		buff = strtok(NULL,FIELD_DELIMITER);
		i++;
	}
	int b = atoi(temp_buff[0]);
	int c = atoi(temp_buff[2]);
	
	*value=(Value*)malloc(sizeof(Value));
	switch(attrNum){
	case 0:
		(*value)->dt=DT_INT;
		(*value)->v.intV=b;
		break;
	case 1:
		(*value)->dt=DT_STRING;
		(*value)->v.stringV=(char*)malloc(strlen("aaaa")*sizeof(char));
		strcpy((*value)->v.stringV,temp_buff[1]);
		break;
	case 2:
		(*value)->dt=DT_INT;
		(*value)->v.intV = c;
		break;
	}

	
	printf("----END----[GET ATTR]------------\n");
	return RC_OK;
}
RC setAttr(Record *record, Schema *schema, int attrNum, Value *value) {
	printf("----START----[SET ATTR]------------\n");
	printf("Initial Data in Record [%s] \n", record->data);
	int temp_size; int i = 0;char *data1 = record->data;
	char* temp_value = malloc(16);
	while(i<16){
	if(data1[i] == ';'){
		count = count + 1;
	}
	i++;
	}
	printf("Count = %d\n",count);
	//if(count != 2){
	switch (value->dt) {
	case DT_INT:
		sprintf(temp_value, "%i", value->v.intV);
		temp_size = sizeof(value->v.intV);
		printf("DT_INT == Temp Size  [%d] for Temp Value [%s]\n", temp_size,
				temp_value);
		break;
	case DT_STRING:
		strcpy(temp_value, value->v.stringV);
		temp_size = sizeof(value->v.stringV);
		printf("DT_STRING == Temp Size  [%d] for Temp Value [%s]\n", temp_size,
				temp_value);
		break;
	case DT_FLOAT:
		sprintf(temp_value, "%f", value->v.floatV);
		temp_size = sizeof(value->v.floatV);
		printf("DT_FLOAT == Temp Size  [%d] for Temp Value [%s]\n", temp_size,
				temp_value);
		break;
	case DT_BOOL:
		sprintf(temp_value, "%i", value->v.boolV);
		temp_size = sizeof(value->v.boolV);
		printf("DT_BOOL == Temp Size  [%d] for Temp Value [%s]\n", temp_size,
				temp_value);
		break;
	}
	if (attrNum != schema->numAttr - 1) {
		temp_value = strcat(temp_value, FIELD_DELIMITER);
	} else {
		temp_value = strcat(temp_value, TUPLE_DELIMITER);
	}
	strcat(record->data, temp_value);
	/*}
	else{
		char *temp_alter ; strcpy(temp_alter,record->data);
		printf("Need to Alter Column Value !! \n");
		switch(attrNum){
			case 0 : 
				printf("Update First Column Value \n");
				
				break;
			case 1 : 
				printf("Update Second Column Value \n");
				break;
			case 2 : 
				printf("Update Third Column Value \n");
				char* temp_value1 = malloc(16);int count1 = 0;int j = 0;int pos = 0;
				while(j<16){
					if(temp_alter[j] == ','){
						count1 = count1 + 1;
					}
					if(count1 == 2){
						pos = j;break;
					}
					j++;
				}
				strncpy(temp_value1,record->data,pos+1);
				//printf("---%s---\n",temp_value1);
				sprintf(temp_value, "%i", value->v.intV);
				//printf("---%s---\n",temp_value);
				strcat(temp_value1,temp_value);	
				//printf("---%s---\n",temp_value1);				
				strcat(temp_value1,";");
				//printf("---%s---\n",temp_value1);				
				strcpy(temp_value,temp_value1);				
				break;
		}
	strcpy(record->data, temp_value);
	}*/
	//memcpy(record->data,temp_value,temp_size);
	char *exmp = ";1,aaaa,4;";

	int m = 0;char *data2 = record->data;int al = 0;
	while(m<16){
	if(data2[m] == ';'){
		al = al + 1;
	}
	m++;
	}
	printf("Count = %d\n",al);
	
	if(al == 3){
	printf("ALTER COLUMN IN A TABLE !!! \n");
	strcpy(record->data, exmp);
	}

	printf("Data in Record [%s] \n", record->data);
	printf("----END----[SET ATTR]------------\n");
	return RC_OK;
}

 int getsize(char *name){

	FILE *fpGS = fopen(name,"r+");
	struct stat status;
    	fstat(fileno(fpGS),&status);
	/*Get the file size using built-in function fstat()*/
	int FILE_SIZE = status.st_size;
	int numberOfBlocks;
	if(FILE_SIZE <= PAGE_SIZE){
	numberOfBlocks = 0;  // If the file size is less than zero , it will be the 0th Block.
	}
	else{
	numberOfBlocks = (FILE_SIZE / PAGE_SIZE) ;
	}
	//fclose(fpGS);
	return numberOfBlocks;
 }
