PRAGMA journal_mode=WAL;

CREATE Table Config (
	`Key` varchar,
	`Value` varchar,
	PRIMARY KEY (`Key`)
);

CREATE Table Inverters (
	Serial int NOT NULL,
	Name varchar,
	Type varchar,
	SW_Version varchar,
	TimeStamp datetime,
	TotalPac int,
	EToday int,
	ETotal int,
	OperatingTime double,
	FeedInTime double,
	Status varchar,
	GridRelay varchar,
	Temperature float,
	PvoSystemSize int,
	PvoInstallDate varchar,
	PRIMARY KEY (Serial)
);

CREATE View vwInverters AS
	SELECT Serial,
	Name,Type,SW_Version,
	datetime(TimeStamp, 'unixepoch', 'localtime') AS TimeStamp,
	TotalPac,
	EToday,ETotal,
	OperatingTime,FeedInTime,
	Status,GridRelay,
	Temperature,
	PvoSystemSize, PvoInstallDate
	FROM Inverters;

CREATE Table SpotData (
	TimeStamp datetime NOT NULL,
	Serial int NOT NULL,
	Pdc1 int, Pdc2 int,
	Idc1 float, Idc2 float,
	Udc1 float, Udc2 float,
	Pac1 int, Pac2 int, Pac3 int,
	Iac1 float, Iac2 float, Iac3 float,
	Uac1 float, Uac2 float, Uac3 float,
	EToday bigint, ETotal bigint,
	Frequency float,
	OperatingTime double,
	FeedInTime double,
	BT_Signal float,
	Status varchar,
	GridRelay varchar,
	Temperature float,
	PRIMARY KEY (TimeStamp, Serial)
);

CREATE View vwSpotData AS
SELECT datetime(Dat.TimeStamp, 'unixepoch', 'localtime') AS TimeStamp, 
	datetime(Dat.TimeStamp - (Dat.TimeStamp % 300)) AS Nearest5min,
	Inv.Name,
	Inv.Type,
	Dat.Serial,
	Pdc1,Pdc2,
	Idc1,Idc2,
	Udc1,Udc2,
	Pac1,Pac2,Pac3,
	Iac1,Iac2,Iac3,
	Uac1,Uac2,Uac3,
	Pdc1+Pdc2 AS PdcTot,
	Pac1+Pac2+Pac3 AS PacTot,
	CASE WHEN Pdc1+Pdc2 = 0 THEN
	    0
	ELSE
	    CASE WHEN Pdc1+Pdc2>Pac1+Pac2+Pac3 THEN
	        ROUND(1.0*(Pac1+Pac2+Pac3)/(Pdc1+Pdc2)*100,1)
	    ELSE
	        100.0
	    END
	END AS Efficiency,
	Dat.EToday,
	Dat.ETotal,
	Frequency,
	Dat.OperatingTime,
	Dat.FeedInTime,
	ROUND(BT_Signal,1) AS BT_Signal,
	Dat.Status,
	Dat.GridRelay,
	ROUND(Dat.Temperature,1) AS Temperature
	FROM [SpotData] Dat
INNER JOIN Inverters Inv ON Dat.Serial = Inv.Serial
ORDER BY Dat.Timestamp Desc;


CREATE Table DayData (
	TimeStamp datetime NOT NULL,
	Serial int NOT NULL,
	TotalYield bigint,
	Power bigint,
	PVoutput tinyint,
	PRIMARY KEY (TimeStamp, Serial)
);

CREATE View vwDayData AS
	select datetime(Dat.TimeStamp, 'unixepoch', 'localtime') AS TimeStamp,
	Inv.Name, Inv.Type, Dat.Serial,
	TotalYield,
	Power,
	PVOutput FROM DayData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial
ORDER BY Dat.Timestamp Desc;

CREATE Table MonthData (
	TimeStamp datetime NOT NULL,
	Serial int NOT NULL,
	TotalYield bigint,
	DayYield bigint,
	PRIMARY KEY (TimeStamp, Serial)
);

CREATE View vwMonthData AS
	select date(Dat.TimeStamp, 'unixepoch') AS TimeStamp,
	Inv.Name, Inv.Type, Dat.Serial,
	TotalYield, DayYield FROM MonthData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial
ORDER BY Dat.Timestamp Desc;

CREATE Table EventData (
	EntryID int,
	TimeStamp datetime NOT NULL,
	Serial int NOT NULL,
	SusyID smallint,
	EventCode int,
	EventType varchar,
	Category varchar,
	EventGroup varchar,
	Tag varchar,
	OldValue varchar,
	NewValue varchar,
	UserGroup varchar,
	PRIMARY KEY (Serial, EntryID)
);

CREATE View vwEventData AS
	select datetime(Dat.TimeStamp, 'unixepoch', 'localtime') AS TimeStamp,
	Inv.Name, Inv.Type, Dat.Serial,
	SusyID, EntryID,
	EventCode, EventType,
	Category, EventGroup, Tag,
	OldValue, NewValue,
	UserGroup FROM EventData Dat
INNER JOIN Inverters Inv ON Dat.Serial=Inv.Serial
ORDER BY EntryID Desc;

CREATE Table Consumption (
	TimeStamp datetime NOT NULL,
	EnergyUsed int,
	PowerUsed int,
	PRIMARY KEY (TimeStamp)
);

CREATE VIEW vwConsumption AS
SELECT datetime(TimeStamp, 'unixepoch', 'localtime') AS TimeStamp, 
	datetime(TimeStamp - (TimeStamp % 300)) AS Nearest5min,
	EnergyUsed,
	PowerUsed
	FROM Consumption;

CREATE VIEW vwAvgConsumption AS
	SELECT Timestamp,
		Nearest5min,
		avg(EnergyUsed) As EnergyUsed,
		avg(PowerUsed) As PowerUsed
	FROM vwConsumption
	GROUP BY Nearest5Min;

CREATE VIEW vwAvgSpotData AS
       SELECT nearest5min,
              serial,
              avg(Pdc1) AS Pdc1,
              avg(Pdc2) AS Pdc2,
              avg(Idc1) AS Idc1,
              avg(Idc2) AS Idc2,
              avg(Udc1) AS Udc1,
              avg(Udc2) AS Udc2,
              avg(Pac1) AS Pac1,
              avg(Pac2) AS Pac2,
              avg(Pac3) AS Pac3,
              avg(Iac1) AS Iac1,
              avg(Iac2) AS Iac2,
              avg(Iac3) AS Iac3,
              avg(Uac1) AS Uac1,
              avg(Uac2) AS Uac2,
              avg(Uac3) AS Uac3,
              avg(Temperature) AS Temperature
        FROM vwSpotData
        GROUP BY serial, nearest5min;

CREATE VIEW vwPvoData AS
       SELECT dd.Timestamp,
              dd.Name,
              dd.Type,
              dd.Serial,
              dd.TotalYield AS V1,
              dd.Power AS V2,
              cons.EnergyUsed AS V3,
              cons.PowerUsed AS V4,
              spot.Temperature AS V5,
              spot.Uac1 AS V6,
              NULL AS V7,
              NULL AS V8,
              NULL AS V9,
              NULL AS V10,
              NULL AS V11,
              NULL AS V12,
              dd.PVoutput
         FROM vwDayData AS dd
              LEFT JOIN vwAvgSpotData AS spot
                     ON dd.Serial = spot.Serial AND dd.Timestamp = spot.Nearest5min
              LEFT JOIN vwAvgConsumption AS cons
                     ON dd.Timestamp = cons.Nearest5min
        ORDER BY dd.Timestamp DESC;

-- Fix 09-JAN-2017 See Issue 54: SQL Support for battery inverters
-- Update_340_SQLite.sql
--
-- SchemaVersion 2
--
-- This version update was forgotten - See issue #335
-- To avoid possible issues, we will not increment the schema version
-- UPDATE Config SET `Value` = '2' WHERE `Key` = 'SchemaVersion'

CREATE TABLE SpotDataX (
    [TimeStamp] INT NOT NULL,
    [Serial]    INT NOT NULL,
    [Key]       INT NOT NULL,
    [Value]     INT,
    PRIMARY KEY (
        [TimeStamp] ASC,
        [Serial] ASC,
        [Key]
    )
)
WITHOUT ROWID;

DROP VIEW IF EXISTS vwBatteryData;

CREATE VIEW vwBatteryData AS
    SELECT DATETIME(sdx.[TimeStamp] - (sdx.[TimeStamp] % 300), 'unixepoch', 'localtime') AS [5min],
           sdx.[Serial],
           inv.[Name],
           AVG(CASE WHEN [Key] = 10586 THEN [Value] END) AS ChaStatus,
           AVG(CASE WHEN [Key] = 18779 THEN CAST([Value] AS REAL) / 10 END) AS Temperature,
           AVG(CASE WHEN [Key] = 18781 THEN CAST([Value] AS REAL) / 1000 END) AS ChaCurrent,
           AVG(CASE WHEN [Key] = 18780 THEN CAST([Value] AS REAL) / 100 END) AS ChaVoltage,
           AVG(CASE WHEN [Key] = 17974 THEN [Value] END) AS GridMsTotWOut,
           AVG(CASE WHEN [Key] = 17975 THEN [Value] END) AS GridMsTotWIn
      FROM SpotDataX AS sdx
           INNER JOIN
           Inverters AS inv ON sdx.[Serial] = inv.[Serial]
     GROUP BY [5min];

-- V3.8.0 Fix Issue #283 SBFspot Upload Performance
-- Update_V3_SQLite.sql
--
-- SchemaVersion 3
--

--
-- vwPVODayData View
--
DROP VIEW IF EXISTS vwPVODayData;

CREATE VIEW vwPVODayData AS
	SELECT
		TimeStamp,
        Serial,
        TotalYield,
        Power
    FROM DayData Dat
    WHERE TimeStamp > strftime( '%s', 'now' ) -( SELECT Value FROM Config WHERE [Key] = 'Batch_DateLimit' ) * 86400 
		AND PvOutput IS NULL;

--
-- vwPVOSpotData View
--
DROP VIEW IF EXISTS vwPVOSpotData;

CREATE VIEW vwPVOSpotData AS
	SELECT
		TimeStamp,
        TimeStamp - (TimeStamp % 300) AS Nearest5min,
        Serial,
        Pdc1,
        Pdc2,
        Idc1,
        Idc2,
        Udc1,
        Udc2,
        Pac1,
        Pac2,
        Pac3,
        Iac1,
        Iac2,
        Iac3,
        Uac1,
        Uac2,
        Uac3,
        Pdc1 + Pdc2 AS PdcTot,
        Pac1 + Pac2 + Pac3 AS PacTot,
        ROUND( Temperature, 1 ) AS Temperature
	FROM SpotData
    WHERE TimeStamp > strftime( '%s', 'now' ) -( SELECT Value FROM Config WHERE [Key] = 'Batch_DateLimit' ) * 86400;

--
-- vwPVOSpotDataAvg View
--
DROP VIEW IF EXISTS vwPVOSpotDataAvg;

CREATE VIEW vwPVOSpotDataAvg AS
	SELECT
		nearest5min,
        serial,
        avg( Pdc1 ) AS Pdc1,
        avg( Pdc2 ) AS Pdc2,
        avg( Idc1 ) AS Idc1,
        avg( Idc2 ) AS Idc2,
        avg( Udc1 ) AS Udc1,
        avg( Udc2 ) AS Udc2,
        avg( Pac1 ) AS Pac1,
        avg( Pac2 ) AS Pac2,
        avg( Pac3 ) AS Pac3,
        avg( Iac1 ) AS Iac1,
        avg( Iac2 ) AS Iac2,
        avg( Iac3 ) AS Iac3,
        avg( Uac1 ) AS Uac1,
        avg( Uac2 ) AS Uac2,
        avg( Uac3 ) AS Uac3,
        avg( Temperature ) AS Temperature
	FROM vwPVOSpotData
    GROUP BY serial, nearest5min;

--
-- vwPVOUploadGeneration View
--
DROP VIEW IF EXISTS vwPVOUploadGeneration;

CREATE VIEW vwPVOUploadGeneration AS
	SELECT
		datetime( dd.TimeStamp, 'unixepoch', 'localtime' ) AS TimeStamp,
		dd.Serial,
		dd.TotalYield AS V1,
		CASE WHEN dd.Power > (IFNULL(inv.SystemSize * 1.4, dd.Power))
			THEN 0 ELSE dd.Power END AS V2,
		NULL AS V3,
		NULL AS V4,
		CASE (SELECT Value FROM Config WHERE [Key] = 'PvoTemperature')
			WHEN 'Ambient' THEN NULL ELSE spot.Temperature END AS V5,
		spot.Uac1 AS V6,
		NULL AS V7,
		NULL AS V8,
		NULL AS V9,
		NULL AS V10,
		NULL AS V11,
		NULL AS V12
	FROM vwPVODayData AS dd
    LEFT JOIN vwPVOSpotDataAvg AS spot ON dd.Serial = spot.Serial AND dd.Timestamp = spot.Nearest5min
	LEFT JOIN Inverters AS inv ON dd.Serial = inv.Serial;


INSERT INTO Config VALUES('SchemaVersion','3');
