-- Роли базы и входы SQL Server.
-- Приложение своих паролей не хранит: вход выполняется логином SQL Server,
-- а роль оно спрашивает у СУБД через IS_MEMBER. Права режет сама база.
-- Выполнять от sysadmin: CREATE LOGIN — это действие на уровне сервера.

USE master;
GO

IF SUSER_ID(N'os_buh') IS NULL
    CREATE LOGIN os_buh WITH PASSWORD = N'Buh12345!', CHECK_POLICY = OFF;
GO
IF SUSER_ID(N'os_viewer') IS NULL
    CREATE LOGIN os_viewer WITH PASSWORD = N'View12345!', CHECK_POLICY = OFF;
GO

USE AssetOs;
GO

IF DATABASE_PRINCIPAL_ID(N'os_accountant') IS NULL
    CREATE ROLE os_accountant;
GO
IF DATABASE_PRINCIPAL_ID(N'os_reader') IS NULL
    CREATE ROLE os_reader;
GO

IF DATABASE_PRINCIPAL_ID(N'os_buh') IS NULL
    CREATE USER os_buh FOR LOGIN os_buh;
GO
IF DATABASE_PRINCIPAL_ID(N'os_viewer') IS NULL
    CREATE USER os_viewer FOR LOGIN os_viewer;
GO

IF IS_ROLEMEMBER(N'os_accountant', N'os_buh') <> 1
    ALTER ROLE os_accountant ADD MEMBER os_buh;
GO
IF IS_ROLEMEMBER(N'os_reader', N'os_viewer') <> 1
    ALTER ROLE os_reader ADD MEMBER os_viewer;
GO

-- Бухгалтер заводит справочники, карточки, начисления и настройки.
-- Карточку он не удаляет: списание — это UPDATE статуса.
GRANT SELECT, INSERT, UPDATE ON dbo.persons   TO os_accountant;
GRANT SELECT, INSERT, UPDATE ON dbo.assets    TO os_accountant;
GRANT SELECT, INSERT, DELETE ON dbo.charges   TO os_accountant;
GRANT SELECT, INSERT         ON dbo.audit_log TO os_accountant;
GRANT SELECT, INSERT, UPDATE ON dbo.settings  TO os_accountant;
DENY  DELETE ON dbo.assets  TO os_accountant;
GO

-- Смотритель только читает.
GRANT SELECT ON dbo.persons   TO os_reader;
GRANT SELECT ON dbo.assets    TO os_reader;
GRANT SELECT ON dbo.charges   TO os_reader;
GRANT SELECT ON dbo.audit_log TO os_reader;
GRANT SELECT ON dbo.settings  TO os_reader;
DENY  INSERT, UPDATE, DELETE ON dbo.assets  TO os_reader;
DENY  INSERT, UPDATE, DELETE ON dbo.charges TO os_reader;
DENY  INSERT, UPDATE, DELETE ON dbo.persons TO os_reader;
DENY  INSERT, UPDATE, DELETE ON dbo.settings TO os_reader;
GO

CREATE OR ALTER TRIGGER dbo.trg_asset_ins
ON dbo.assets
AFTER INSERT
AS
BEGIN
    SET NOCOUNT ON;
    INSERT INTO dbo.audit_log(user_name, action, details)
    SELECT N'триггер', N'карточка создана', i.inv_no + N' ' + i.title
    FROM inserted AS i;
END
GO

-- Отдельную строку начисления не переписываю: месяц либо закрыт, либо отменён целиком.
-- Отмена закрытия удаляет проводки месяца одной операцией, и это пишется в audit_log.
CREATE OR ALTER TRIGGER dbo.trg_charge_lock
ON dbo.charges
AFTER UPDATE
AS
BEGIN
    SET NOCOUNT ON;
    THROW 51001, N'Строку начисленной амортизации не переписываю. Если месяц закрыт с ошибкой, отмените закрытие целиком и закройте заново.', 1;
END
GO

