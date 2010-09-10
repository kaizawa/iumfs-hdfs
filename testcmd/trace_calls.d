#pragma D option flowindent

fbt:iumfs::entry
{
    @[probefunc] = count();
    printf("tid=%d", tid);    

}

fbt:iumfs::return
{
   printf("tid=%d", tid);  
}
