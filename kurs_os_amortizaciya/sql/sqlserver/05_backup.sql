-- Копия базы. Путь должен быть доступен службе SQL Server, не вашему рабочему столу.
-- Берём папку резервных копий по умолчанию, чтобы не угадывать диск:
-- её же использует пункт меню «резервная копия», если путь не указан.

USE master;
GO

DECLARE @dir NVARCHAR(400) = CAST(SERVERPROPERTY(N'InstanceDefaultBackupPath') AS NVARCHAR(400));
IF @dir IS NULL OR @dir = N''
BEGIN
    THROW 51002, N'Сервер не сообщил папку резервных копий, укажите путь вручную.', 1;
END
IF RIGHT(@dir, 1) <> N'\'
    SET @dir = @dir + N'\';

DECLARE @sql NVARCHAR(MAX) =
    N'BACKUP DATABASE AssetOs TO DISK = N''' + @dir + N'AssetOs.bak''
      WITH INIT, COPY_ONLY, STATS = 10, NAME = N''AssetOs учебная копия'';';
EXEC (@sql);
GO
