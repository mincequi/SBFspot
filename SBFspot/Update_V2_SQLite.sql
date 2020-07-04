-- Update_340_SQLite.sql
-- SchemaVersion 2
--
-- Fix 09-JAN-2017 See Issue 54: SQL Support for battery inverters
--

CREATE TABLE IF NOT EXISTS SpotDataX (
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

-- This version update was forgotten - See issue #335
-- To avoid possible issues, we will not increment the schema version
-- UPDATE Config SET `Value` = '2' WHERE `Key` = 'SchemaVersion'
