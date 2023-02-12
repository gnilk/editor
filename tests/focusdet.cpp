//
// Created by gnilk on 11.02.23.
//
#include "focus_oc_wrapper.hpp"
#include <thread>

int main(int argc, const char * argv[])
{
    FocusDetector::AppFocus focus;
    focus.run();

    //std::thread threadListener(&FocusDetector::AppFocus::run, &focus); //Does not works
    //if (threadListener.joinable())
    //{
    //  threadListener.join();
    //}
}