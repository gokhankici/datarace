/*
 * MyDefinitions.h
 *
 *  Created on: Jan 7, 2014
 *      Author: gokhan
 */

#ifndef MYFLAGS_H_
#define MYFLAGS_H_

// when defined, replace the bloom filter with a std::set
//#define SET_OVERRIDE

// when enabled, print the debugging messages to the screen
//#define DEBUG_MODE

//#define PRINT_DETAILED_RACE_INFO

//#define WRITE_ADDRESSES_TO_LOG_FILE

#define ON_MY_MACHINE

#define PRINT_ONLY_RACE_RELATED

#ifdef  PRINT_ONLY_RACE_RELATED
#define SET_OVERRIDE
#define PRINT_DETAILED_RACE_INFO
#endif

//#define PRINT_SYNC_FUNCTION

#define PRINT_SIGNATURES

#endif /* MYDEFINITIONS_H_ */
