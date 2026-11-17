-- Те же учебные карточки, что заводит команда osmod --demo.
-- kind: 1 основное средство, 2 малоценка.
-- status: 1 в учёте, 2 списано.
-- method: 1 линейный, 2 уменьшаемого остатка, 3 по выработке, 4 по сумме чисел лет.

USE AssetOs;
GO

MERGE dbo.settings AS t
USING (VALUES
    (N'company',    N'ООО «Кровля-Сервис»'),
    (N'limit_kop',  N'10000000'),
    (N'start_rule', N'со следующего месяца после принятия')
) AS s(skey, svalue)
ON t.skey = s.skey
WHEN MATCHED THEN UPDATE SET svalue = s.svalue
WHEN NOT MATCHED THEN INSERT (skey, svalue) VALUES (s.skey, s.svalue);
GO

IF NOT EXISTS (SELECT 1 FROM dbo.persons WHERE id = 1)
    INSERT INTO dbo.persons(full_name, job_title) VALUES (N'Соколов Игорь Петрович', N'директор');
IF NOT EXISTS (SELECT 1 FROM dbo.persons WHERE full_name = N'Фомин Артём Сергеевич')
    INSERT INTO dbo.persons(full_name, job_title) VALUES (N'Фомин Артём Сергеевич', N'прораб');
IF NOT EXISTS (SELECT 1 FROM dbo.persons WHERE full_name = N'Нуриева Дина Рашидовна')
    INSERT INTO dbo.persons(full_name, job_title) VALUES (N'Нуриева Дина Рашидовна', N'кладовщик');
GO

IF NOT EXISTS (SELECT 1 FROM dbo.assets WHERE inv_no = N'ОС-0001')
INSERT INTO dbo.assets(
    inv_no, title, person_id, kind, status, method, accepted_on,
    cost_kop, liq_kop, life_months, decline_x100, plan_units, place, note)
SELECT N'ОС-0001', N'Газель Next, гос. номер А323ММ 750', p.id, 1, 1, 2, '2024-03-18',
       164000000, 24000000, 60, 200, 0, N'стоянка у ангара, Подольск',
       N'коэффициент 2 закреплён в учётной политике на 2024 год'
FROM dbo.persons AS p WHERE p.full_name = N'Соколов Игорь Петрович';
GO

IF NOT EXISTS (SELECT 1 FROM dbo.assets WHERE inv_no = N'ОС-0002')
INSERT INTO dbo.assets(
    inv_no, title, person_id, kind, status, method, accepted_on,
    cost_kop, liq_kop, life_months, decline_x100, plan_units, place, note)
SELECT N'ОС-0002', N'Компрессор Aurora Storm 90', p.id, 1, 1, 1, '2025-01-20',
       18640000, 640000, 24, 0, 0, N'бокс 2',
       N'принимали по накладной 14, без отдельной доставки'
FROM dbo.persons AS p WHERE p.full_name = N'Фомин Артём Сергеевич';
GO

IF NOT EXISTS (SELECT 1 FROM dbo.assets WHERE inv_no = N'ОС-0003')
INSERT INTO dbo.assets(
    inv_no, title, person_id, kind, status, method, accepted_on,
    cost_kop, liq_kop, life_months, decline_x100, plan_units, place, note)
SELECT N'ОС-0003', N'Люлька фасадная ZLP-630', p.id, 1, 1, 3, '2024-06-02',
       51000000, 3000000, 48, 0, 12000, N'объект, когда не в боксе',
       N'выработка в моточасах, план 12000'
FROM dbo.persons AS p WHERE p.full_name = N'Фомин Артём Сергеевич';
GO

IF NOT EXISTS (SELECT 1 FROM dbo.assets WHERE inv_no = N'МЦ-0014')
INSERT INTO dbo.assets(
    inv_no, title, person_id, kind, status, method, accepted_on,
    cost_kop, liq_kop, life_months, decline_x100, plan_units, place, note)
SELECT N'МЦ-0014', N'Ноутбук бухгалтера Honor', p.id, 2, 1, 1, '2025-09-03',
       6700000, 0, 0, 0, 0, N'офис, стол у окна',
       N'ниже лимита 100000, амортизацию не крутим'
FROM dbo.persons AS p WHERE p.full_name = N'Нуриева Дина Рашидовна';
GO

IF NOT EXISTS (SELECT 1 FROM dbo.assets WHERE inv_no = N'МЦ-0015')
INSERT INTO dbo.assets(
    inv_no, title, person_id, kind, status, method, accepted_on,
    cost_kop, liq_kop, life_months, decline_x100, plan_units, place, note)
SELECT N'МЦ-0015', N'Перфоратор DeWalt', p.id, 2, 1, 1, '2025-04-11',
       1899000, 0, 0, 0, 0, N'кладовая, стеллаж 3',
       N'инструмент ниже лимита'
FROM dbo.persons AS p WHERE p.full_name = N'Фомин Артём Сергеевич';
GO
