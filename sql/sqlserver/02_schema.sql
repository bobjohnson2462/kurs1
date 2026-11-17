-- Таблицы модуля. Имена совпадают с тем, что создаёт SqlServerStore::migrate(),
-- кроме того что здесь удобнее смотреть скрипт глазами в SSMS.
-- Повторный запуск не роняет уже созданные таблицы.

USE AssetOs;
GO

IF OBJECT_ID(N'dbo.persons', N'U') IS NULL
CREATE TABLE dbo.persons (
    id          BIGINT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    full_name   NVARCHAR(200) NOT NULL,
    job_title   NVARCHAR(120) NOT NULL
);
GO

IF OBJECT_ID(N'dbo.assets', N'U') IS NULL
CREATE TABLE dbo.assets (
    id            BIGINT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    inv_no        NVARCHAR(20)  NOT NULL UNIQUE,
    title         NVARCHAR(200) NOT NULL,
    person_id     BIGINT        NOT NULL REFERENCES dbo.persons(id),
    kind          INT           NOT NULL,
    status        INT           NOT NULL,
    method        INT           NOT NULL CONSTRAINT ck_assets_method CHECK (method BETWEEN 1 AND 4),
    accepted_on   CHAR(10)      NOT NULL,
    cost_kop      BIGINT        NOT NULL CHECK (cost_kop >= 0),
    liq_kop       BIGINT        NOT NULL CHECK (liq_kop >= 0),
    life_months   INT           NOT NULL,
    decline_x100  INT           NOT NULL,
    plan_units    BIGINT        NOT NULL CONSTRAINT df_plan DEFAULT 0,
    place         NVARCHAR(200) NOT NULL CONSTRAINT df_place DEFAULT N'',
    note          NVARCHAR(400) NOT NULL CONSTRAINT df_note DEFAULT N'',
    retired_on    CHAR(10)      NULL,
    retire_reason NVARCHAR(300) NULL
);
GO

IF OBJECT_ID(N'dbo.charges', N'U') IS NULL
CREATE TABLE dbo.charges (
    id          BIGINT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    asset_id    BIGINT NOT NULL REFERENCES dbo.assets(id),
    y           INT    NOT NULL,
    m           INT    NOT NULL CHECK (m BETWEEN 1 AND 12),
    amount_kop  BIGINT NOT NULL,
    book_kop    BIGINT NOT NULL,
    units       BIGINT NOT NULL CONSTRAINT df_units DEFAULT 0,
    CONSTRAINT uq_charge UNIQUE (asset_id, y, m)
);
GO

IF OBJECT_ID(N'dbo.audit_log', N'U') IS NULL
CREATE TABLE dbo.audit_log (
    id         BIGINT IDENTITY(1,1) NOT NULL PRIMARY KEY,
    ts         DATETIME2 NOT NULL CONSTRAINT df_ts DEFAULT SYSDATETIME(),
    user_name  NVARCHAR(80)  NOT NULL,
    action     NVARCHAR(80)  NOT NULL,
    details    NVARCHAR(400) NOT NULL
);
GO

IF OBJECT_ID(N'dbo.settings', N'U') IS NULL
CREATE TABLE dbo.settings (
    skey    NVARCHAR(40)  NOT NULL PRIMARY KEY,
    svalue  NVARCHAR(200) NOT NULL
);
GO

IF NOT EXISTS (SELECT 1 FROM sys.indexes WHERE name = N'ix_charges_asset' AND object_id = OBJECT_ID(N'dbo.charges'))
    CREATE INDEX ix_charges_asset ON dbo.charges(asset_id);
GO

IF NOT EXISTS (SELECT 1 FROM sys.indexes WHERE name = N'ix_assets_person' AND object_id = OBJECT_ID(N'dbo.assets'))
    CREATE INDEX ix_assets_person ON dbo.assets(person_id);
GO

-- Для баз, созданных до появления четвёртого способа: добираем ограничение.
IF OBJECT_ID(N'dbo.assets', N'U') IS NOT NULL
   AND NOT EXISTS (SELECT 1 FROM sys.check_constraints
                   WHERE name = N'ck_assets_method' AND parent_object_id = OBJECT_ID(N'dbo.assets'))
    ALTER TABLE dbo.assets WITH CHECK
        ADD CONSTRAINT ck_assets_method CHECK (method BETWEEN 1 AND 4);
GO
