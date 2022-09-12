*** Settings ***
Resource	../resources/resources.robot
Suite Setup	Clean Before Suite
Suite Teardown	Clean After Suite
Test Setup	Stop Processes
Test Teardown	Save logs If Failed

Documentation	Centreon Broker and Engine anomaly detection


Library	DateTime
Library	Process
Library	OperatingSystem
Library	../resources/Engine.py
Library	../resources/Broker.py
Library	../resources/Common.py


*** Test Cases ***
ANO_NOFILE
	[Documentation]	an anomaly detection without threshold file must be in unknown state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
    ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
    Remove File  /tmp/anomaly_threshold.json
    Clear Retention
    Clear Db  services
    Start Broker
    Start Engine
    Process Service Check result	host_1	anomaly_${serv_id}	2	taratata
    Check Service Status With Timeout  host_1  anomaly_${serv_id}  3  30
    Stop Broker
    Stop Engine

ANO_TOO_OLD_FILE
	[Documentation]	an anomaly detection with an oldest threshold file must be in unknown state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
    ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
    ${predict_data} =  Evaluate  [[0,0,2],[1648812678,0,3]]
    Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
    Clear Retention
    Clear Db  services
    Start Broker
    Start Engine
    Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=70%;50;75
    Check Service Status With Timeout  host_1  anomaly_${serv_id}  3  30
    Stop Broker
    Stop Engine


ANO_OUT_LOWER_THAN_LIMIT
	[Documentation]	an anomaly detection with a perfdata lower than lower limit make a critical state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
    ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
    ${predict_data} =  Evaluate  [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
    Clear Retention
    Clear Db  services
    Start Broker
    Start Engine
    Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=20%;50;75
    Check Service Status With Timeout  host_1  anomaly_${serv_id}  2  30
    Stop Broker
    Stop Engine

ANO_OUT_UPPER_THAN_LIMIT
	[Documentation]	an anomaly detection with a perfdata upper than upper limit make a critical state
	[Tags]  Broker	Engine  Anomaly
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
    ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
    ${predict_data} =  Evaluate  [[0,50,52],[2648812678,50,63]]
    Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
    Clear Retention
    Clear Db  services
    Start Broker
    Start Engine
    Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=80%;50;75
    Check Service Status With Timeout  host_1  anomaly_${serv_id}  2  30
    Stop Broker
    Stop Engine

ANO_JSON_SENSITIVITY_NOT_SAVED
	[Documentation]	json sensitivity not saved in retention
	[Tags]  Engine  Anomaly  Retention
	Config Engine	${1}	${50}	${20}
    ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
    ${predict_data} =  Evaluate  [[0,50,2, 10],[2648812678,25,-5,6]]
    Create Anomaly Threshold File V2  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  55.0  ${predict_data}
    Clear Retention
    Start Engine
    Sleep  5s
    Stop Engine
    ${retention_sensitivity} =  Grep Retention  ${0}  sensitivity=0.00
    Should Be Equal As Strings	${retention_sensitivity}	sensitivity=0.00 

ANO_CFG_SENSITIVITY_SAVED
	[Documentation]	cfg sensitivity saved in retention
	[Tags]  Engine  Anomaly  Retention
	Config Engine	${1}	${50}	${20}
    ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric  4.00
    ${predict_data} =  Evaluate  [[0,50,2, 10],[2648812678,25,-5,6]]
    Create Anomaly Threshold File V2  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  55.0  ${predict_data}
    Clear Retention
    Start Engine
    Sleep  5s
    Stop Engine
    ${retention_sensitivity} =  Grep Retention  ${0}  sensitivity=4.00
    Should Be Equal As Strings	${retention_sensitivity}	sensitivity=4.00

ANO_EXTCMD_SENSITIVITY_SAVED
	[Documentation]	extcmd sensitivity saved in retention
	[Tags]  Engine  Anomaly  Retention  extcmd
  	FOR  ${use_grpc}  IN RANGE  1  2
        Config Engine	${1}	${50}	${20}
        ${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
        ${predict_data} =  Evaluate  [[0,50,2, 10],[2648812678,25,-5,6]]
        Create Anomaly Threshold File V2  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  55.0  ${predict_data}
        Clear Retention
        Start Engine
        Sleep  5s
        Update Ano Sensitivity  ${use_grpc}  host_1  anomaly_1001  4.55
        Sleep  1s
        Stop Engine
        ${retention_sensitivity} =  Grep Retention  ${0}  sensitivity=4.55
        Should Be Equal As Strings	${retention_sensitivity}	sensitivity=4.55
    END

AOUTLU1
	[Documentation]	an anomaly detection with a perfdata upper than upper limit make a critical state with bbdo 3
	[Tags]  Broker	Engine	Anomaly	bbdo
	Config Engine	${1}	${50}	${20}
	Config Broker	central
	Config Broker	rrd
	Config Broker	module	${1}
	Broker Config Log	central	sql	debug
	Config Broker Sql Output	central	unified_sql
	Broker Config Add Item	module0	bbdo_version	3.0.1
	Broker Config Add Item	rrd	bbdo_version	3.0.1
	Broker Config Add Item	central	bbdo_version	3.0.1
	${serv_id}=  Create Anomaly Detection  ${0}  ${1}  ${1}  metric
	${predict_data} =  Evaluate  [[0,50,52],[2648812678,50,63]]
	Create Anomaly Threshold File  /tmp/anomaly_threshold.json  ${1}  ${serv_id}  metric  ${predict_data}
	Clear Retention
	Clear Db  services
	Start Broker
	${start}=	Get Current Date
	Start Engine
	# Let's wait for the initial service states.
	${content}=	Create List	INITIAL SERVICE STATE: host_50;service_1000;
	${result}=	Find In Log with Timeout	${engineLog0}	${start}	${content}	60
	Should Be True	${result}	msg=An Initial service state on service (50, 1000) should be raised before we can start external commands.

	Process Service Check result	host_1	anomaly_${serv_id}	2	taratata|metric=80%;50;75
	Check Service Status With Timeout  host_1  anomaly_${serv_id}  2  30
	Stop Engine
	Kindly Stop Broker
	${lst}=	Create List	1	0	4
	${result}=	Check Types in resources	${lst}
	Should Be True	${result}	msg=The table 'resources' should contain rows of types SERVICE, HOST and ANOMALY_DETECTION.

