/*
        AXC_FileInputTest include file
        Using file content as test parameter 
*/
#ifndef __AXC_FEEDINGFILEINPUTTEST_H__
#define __AXC_FEEDINGFILEINPUTTEST_H__
#include "AXI_Test.h"
#include "AXI_DoitLater.h"

#define TEST_FILE_PATH "/data/data/%s"
#define MAX_SIZE_OF_TEST_FILE_PATH (32)
struct AXC_FeedingFileInputTest;
struct AXC_FeedingFileInputParser;
struct AXC_FeedingFileInputTest_Impl;

typedef struct AXC_FeedingFileInputParser{
    //Child must implement it...
    void(*parse)(struct AXC_FeedingFileInputParser *test, unsigned int size);
    char *(*getBuffer)(struct AXC_FeedingFileInputParser *test, unsigned int *size);
    //chlid need implement...
}AXC_FeedingFileInputParser;

typedef struct AXC_FeedingFileInputTest_Impl{
    void(*createFileForFeeding)(struct AXC_FeedingFileInputTest *_this,char *buf, unsigned int size);
}AXC_FeedingFileInputTest_Impl;

typedef struct AXC_FeedingFileInputTest{
    AXI_Test miParent;

    char testname[MAX_SIZE_OF_TEST_FILE_PATH];

    AXI_DoitLater *mpDoitLater;

    AXI_TestReport *report;

    AXC_FeedingFileInputParser *parser;

    AXI_DoitLaterTask task;

    AXC_FeedingFileInputTest_Impl mImpl;
    
}AXC_FeedingFileInputTest;

extern void AXC_FeedingFileInputTest_constructor(struct AXC_FeedingFileInputTest *test, 
    const char *testname,
    AXC_FeedingFileInputParser *parser);

#endif //__AXC_FEEDINGFILEINPUTTEST_H__