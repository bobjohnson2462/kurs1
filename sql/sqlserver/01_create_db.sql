-- SSMS 18. Скрипт создаёт пустые базы под модуль.
-- Выполнять от имени учётной записи, которой разрешено CREATE DATABASE.
-- Дальше по порядку: 02_schema.sql, 03_roles_audit.sql, 04_seed.sql.
-- AssetOs      рабочая база, сюда смотрит меню;
-- AssetOsDemo  учебный сценарий osmod --demo;
-- AssetOsTest  проверки osmod_tests.exe, содержимое очищается перед прогоном.

IF DB_ID(N'AssetOs') IS NULL
BEGIN
    CREATE DATABASE AssetOs;
END
GO
IF DB_ID(N'AssetOsDemo') IS NULL
BEGIN
    CREATE DATABASE AssetOsDemo;
END
GO
IF DB_ID(N'AssetOsTest') IS NULL
BEGIN
    CREATE DATABASE AssetOsTest;
END
GO

ALTER DATABASE AssetOs SET RECOVERY SIMPLE;
GO
ALTER DATABASE AssetOsDemo SET RECOVERY SIMPLE;
GO
ALTER DATABASE AssetOsTest SET RECOVERY SIMPLE;
GO
