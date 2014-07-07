#ifdef CONFIG_ASUS_POWER_UTIL 

#include "AXC_FeedingFileInputTest.h"
#include "AXC_DoitLaterFactory.h"
#include <linux/fs.h>
#include <asm/uaccess.h>

static char DEBUG_STR_1_STR_1_INT[] = "[FFITest]%s,%s=%d\n"; 
#define FEEDING_FILE_INPUT_TEST_DEBUG_2_STR_1_INT(_str,_int) \
printk(DEBUG_STR_1_STR_1_INT,   \
__FUNCTION__,   \
_str,   \
_int)

static char DEBUG_STR[] = "[FFITest]%s,%s\n"; 
#define FEEDING_FILE_INPUT_TEST_DEBUG(_str)   \
printk( DEBUG_STR,  \
__FUNCTION__,   \
_str)   

static void AXC_FeedingFileInputTest_test(AXI_Test *test, AXI_TestReport *report)
{
    struct AXC_FeedingFileInputTest  *_this=
        container_of(test, AXC_FeedingFileInputTest, miParent);

    _this->report = report;

    _this->mpDoitLater->start(_this->mpDoitLater, &_this->task, 0 , 1);
}

static void AXC_FeedingFileInputTest_dotask(struct AXI_DoitLaterTask *task)
{
    char *buf;
    unsigned int buf_size;

    struct AXC_FeedingFileInputTest  *_this=
        container_of(task, AXC_FeedingFileInputTest, task);

    struct file *fp_bat_sts = NULL;
   // int used = 0;
    mm_segment_t old_fs;
//    unsigned char bat_cap_s[4];
    //loff_t pos_bat = 0;
    int length = 0;

    buf = _this->parser->getBuffer(_this->parser, &buf_size);
 
    old_fs = get_fs();
    set_fs(KERNEL_DS);

    fp_bat_sts = filp_open(_this->testname, O_RDONLY, 0);
    if (!IS_ERR(fp_bat_sts)) {
        length = fp_bat_sts->f_op->read(fp_bat_sts,
            buf, 
            buf_size, 
            &fp_bat_sts->f_pos);
        set_fs(old_fs);
        filp_close(fp_bat_sts, NULL);
    } else {
        FEEDING_FILE_INPUT_TEST_DEBUG("Can't open file");

        if(NULL != _this->report){

            _this->report->report(_this->report, false);
        }
    }

    _this->parser->parse(_this->parser, length);

    if(NULL != _this->report){

        _this->report->report(_this->report, true);
    }                    
}

static void AXC_FeedingFileInputTest_createFileForFeeding(AXC_FeedingFileInputTest *_this,char *buf, unsigned int size)
{
        struct file *fp_bat_sts = NULL;
        //int used = 0;
        mm_segment_t old_fs;
        //unsigned char bat_cap_s[4];
        //loff_t pos_bat = 0;
        int length = 0;
   
        old_fs = get_fs();
        set_fs(KERNEL_DS);
            
        fp_bat_sts = filp_open(_this->testname, O_WRONLY|O_CREAT,0640);
        if (!IS_ERR(fp_bat_sts)) {
            length = fp_bat_sts->f_op->write(fp_bat_sts,
                buf, 
                size, 
                &fp_bat_sts->f_pos);
            set_fs(old_fs);
            filp_close(fp_bat_sts, NULL);
        } else {
            FEEDING_FILE_INPUT_TEST_DEBUG_2_STR_1_INT("Can't open file,size=",size);
        }
        
}
void AXC_FeedingFileInputTest_constructor(struct AXC_FeedingFileInputTest *_this, 
    const char *testname,
    AXC_FeedingFileInputParser *parser)
{
    _this->miParent.test = AXC_FeedingFileInputTest_test;
    _this->task.dotask = AXC_FeedingFileInputTest_dotask;
    
    BUG_ON(NULL == testname);
    memset(_this->testname,0x00,MAX_SIZE_OF_TEST_FILE_PATH);
    sprintf(_this->testname, TEST_FILE_PATH,testname);
    
    _this->mpDoitLater = get_DoitLater(E_ONE_SHOT_WORKER_TYPE);
    BUG_ON(NULL == _this->mpDoitLater);

    BUG_ON(NULL == parser);
    _this->parser = parser;

    _this->mImpl.createFileForFeeding = AXC_FeedingFileInputTest_createFileForFeeding;
}
#endif//#ifdef CONFIG_ASUS_POWER_UTIL 
