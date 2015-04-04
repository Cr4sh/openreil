#include <string>
#include <stdio.h>
#include <assert.h>

using namespace std;

#include "common.h"

//----------------------------------------------------------------------
// A panic function that prints a msg and terminates the program
//----------------------------------------------------------------------
void panic(string msg)
{
    string str = "Panic: " + msg;

    throw str.c_str();
}
