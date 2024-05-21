// The following code would only be executed once,
// when the program starts.

extern void* _scritical;
extern void* _ecritical;
void Bootstrap_InitCriticalData()
{
    // Set all of the critical section data to 0
    for (void** p = &_scritical; p < &_ecritical; p++)
        *p = 0;
}