//
// Created by gnilk on 30.10.23.
//

#ifndef GOATEDIT_COMPILETIMECONFIG_H
#define GOATEDIT_COMPILETIMECONFIG_H

// Maximum 250ms per poll
// This defines the timeout's for all waits, including queue events, polling of fd's and so forth
#ifndef GEDIT_DEFAULT_POLL_TMO_MS
#define GEDIT_DEFAULT_POLL_TMO_MS 250
#endif



#endif //GOATEDIT_COMPILETIMECONFIG_H
