-- Выборка для ведомости и для проверки, что месяц не закрыт дважды.
-- Копейки делю на 100.00 уже в отчёте, в таблицах они целые.

USE AssetOs;
GO

-- Остаточная стоимость на сейчас.
SELECT
    a.inv_no AS [Инв. номер],
    a.title AS [Наименование],
    CASE a.kind WHEN 1 THEN N'ОС' ELSE N'малоценка' END AS [Вид],
    CASE a.method
        WHEN 1 THEN N'линейный'
        WHEN 2 THEN N'уменьшаемый остаток'
        WHEN 3 THEN N'по выработке'
        WHEN 4 THEN N'по сумме чисел лет'
    END AS [Способ],
    p.full_name AS [МОЛ],
    CAST(a.cost_kop / 100.0 AS DECIMAL(18,2)) AS [Первоначальная],
    CAST(ISNULL(SUM(c.amount_kop), 0) / 100.0 AS DECIMAL(18,2)) AS [Начислено],
    CAST((a.cost_kop - ISNULL(SUM(c.amount_kop), 0)) / 100.0 AS DECIMAL(18,2)) AS [Балансовая]
FROM dbo.assets AS a
JOIN dbo.persons AS p ON p.id = a.person_id
LEFT JOIN dbo.charges AS c ON c.asset_id = a.id
GROUP BY a.inv_no, a.title, a.kind, a.method, p.full_name, a.cost_kop, a.status
ORDER BY a.inv_no;
GO

-- Начисления за конкретный месяц.
DECLARE @y INT = 2025, @m INT = 4;
SELECT
    a.inv_no,
    a.title,
    CAST(c.amount_kop / 100.0 AS DECIMAL(18,2)) AS [За месяц],
    CAST(c.book_kop / 100.0 AS DECIMAL(18,2)) AS [Балансовая после],
    c.units AS [Выработка]
FROM dbo.charges AS c
JOIN dbo.assets AS a ON a.id = c.asset_id
WHERE c.y = @y AND c.m = @m
ORDER BY a.inv_no;
GO

-- Что числится за прорабом.
SELECT a.inv_no, a.title, a.place
FROM dbo.assets AS a
JOIN dbo.persons AS p ON p.id = a.person_id
WHERE p.job_title = N'прораб' AND a.status = 1
ORDER BY a.inv_no;
GO

-- Уже дошли до ликвидационной, но ещё не списаны.
SELECT a.inv_no, a.title,
       CAST(a.liq_kop / 100.0 AS DECIMAL(18,2)) AS [Ликвидационная],
       CAST((a.cost_kop - ISNULL(SUM(c.amount_kop), 0)) / 100.0 AS DECIMAL(18,2)) AS [Балансовая]
FROM dbo.assets AS a
LEFT JOIN dbo.charges AS c ON c.asset_id = a.id
WHERE a.kind = 1 AND a.status = 1
GROUP BY a.inv_no, a.title, a.cost_kop, a.liq_kop
HAVING a.cost_kop - ISNULL(SUM(c.amount_kop), 0) <= a.liq_kop;
GO

-- Сколько начислили по годам. Удобно сверять с оборотами 02 счёта, когда дойдут руки.
SELECT c.y AS [Год],
       CAST(SUM(c.amount_kop) / 100.0 AS DECIMAL(18,2)) AS [Амортизация за год]
FROM dbo.charges AS c
GROUP BY c.y
ORDER BY c.y;
GO

-- Хвост журнала.
SELECT TOP (30) ts, user_name, action, details
FROM dbo.audit_log
ORDER BY id DESC;
GO
