/*
** Copyright 1999-2006 Ethan Galstad
** Copyright 2011      Merethis
**
** This file is part of Centreon Scheduler.
**
** Centreon Scheduler is free software: you can redistribute it and/or
** modify it under the terms of the GNU General Public License version 2
** as published by the Free Software Foundation.
**
** Centreon Scheduler is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with Centreon Scheduler. If not, see
** <http://www.gnu.org/licenses/>.
*/

/*********** COMMON HEADER FILES ***********/

#include "../include/config.hh"
#include "../include/common.hh"
#include "../include/objects.hh"
#include "../include/statusdata.hh"
#include "../include/nagios.hh"
#include "../include/sretention.hh"
#include "../include/broker.hh"

extern int            retain_state_information;



/**** IMPLEMENTATION SPECIFIC HEADER FILES ****/
#ifdef USE_XRDDEFAULT
#include "../xdata/xrddefault.hh"		/* default routines */
#endif






/******************************************************************/
/************* TOP-LEVEL STATE INFORMATION FUNCTIONS **************/
/******************************************************************/


/* initializes retention data at program start */
int initialize_retention_data(char *config_file){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XRDDEFAULT
	result=xrddefault_initialize_retention_data(config_file);
#endif

	return result;
        }



/* cleans up retention data before program termination */
int cleanup_retention_data(char *config_file){
	int result=OK;

	/**** IMPLEMENTATION-SPECIFIC CALLS ****/
#ifdef USE_XRDDEFAULT
	result=xrddefault_cleanup_retention_data(config_file);
#endif

	return result;
        }



/* save all host and service state information */
int save_state_information(int autosave){
	int result=OK;

	if(retain_state_information==FALSE)
		return OK;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_STARTSAVE,NEBFLAG_NONE,NEBATTR_NONE,NULL);
#endif

	/********* IMPLEMENTATION-SPECIFIC OUTPUT FUNCTION ********/
#ifdef USE_XRDDEFAULT
	result=xrddefault_save_state_information();
#endif

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_ENDSAVE,NEBFLAG_NONE,NEBATTR_NONE,NULL);
#endif

	if(result==ERROR)
		return ERROR;

	if(autosave==TRUE)
		logit(NSLOG_PROCESS_INFO,FALSE,"Auto-save of retention data completed successfully.\n");

	return OK;
        }




/* reads in initial host and state information */
int read_initial_state_information(void){
	int result=OK;

	if(retain_state_information==FALSE)
		return OK;

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_STARTLOAD,NEBFLAG_NONE,NEBATTR_NONE,NULL);
#endif

	/********* IMPLEMENTATION-SPECIFIC INPUT FUNCTION ********/
#ifdef USE_XRDDEFAULT
	result=xrddefault_read_state_information();
#endif

#ifdef USE_EVENT_BROKER
	/* send data to event broker */
	broker_retention_data(NEBTYPE_RETENTIONDATA_ENDLOAD,NEBFLAG_NONE,NEBATTR_NONE,NULL);
#endif

	if(result==ERROR)
		return ERROR;

	return OK;
        }



