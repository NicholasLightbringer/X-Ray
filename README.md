Форк рушія ТЧ 1.0007 від xrModder

Ключові відмінності на 07.10.2026:
- Інерція HUD у прицілюванні + можливість налаштування у конфігу конкретної зброї (hud_aim_inertion = true/false, hud_aim_inertion_strength = ...).
- Розкачка HUD як у старих збірках ТЧ + можливість налаштування (нові параметри для [bobbing_effector] у effectors.ltx: hud_bobbing_speed, hud_bobbing_amplitude, hud_bobbing_vertical).
- Рух HUD зброї при стрибках і приземленні + можливість налаштування (нові параметри для [bobbing_effector] у effectors.ltx: hud_jump_down, hud_jump_up, hud_jump_duration, hud_jump_pitch_down, hud_jump_pitch_up, hud_landing_duration, hud_landing_down, hud_landing_pitch_down).
- Можливість скриптово змінювати ім'я та іконку актора (actor:set_actor_name, actor:set_actor_icon) + винесено в xr_effects: change_actor_name, change_actor_icon).
- Відображення текстури-заглушки замість вильоту по "can`t find texture" + так само зроблено і для потенційно поламаних dds.
- Можливість встановити круглий приціл, як у білдах (консольна команда hud_cursor_weapon).
- Нові гарячі клавіші: на швидке використання їжі, антираду, на дію "Взяти все" і дію "Розрядити зброю в інвентарі".
- Назва грошової одиниці винесена у xml (st_money_name).
- Можливість прописати параметр use_sound для юзабельних предметів (програвання предметом звуку).
- Можливість налаштовувати переходи на локації у спеціальному конфігу (level_changers.ltx). Вказується секція переходу що = назві його спавн-секції, а також параметри closed = {+cond} true, false (можна заборонити/дозволити перехід видачею інфопорції), tip_closed = ... (текст, що висвітиться на екрані переходу, якщо він закритий (за замовчуванням st_level_changer_disabled), tip_opened = ... (текст на екрані, якщо перехід відкритий (за замовч. дефолтний level_changer_invitation).
- Додано підтримку віконного режиму без рамок (rs_borderless) + додано можливість перемикатися між 3 доступними режимами (rs_display_mode).
- Нові властивості для артефактів: додаткова переносима вага (additional_weight), швидкість бігу (sprint_speed), висота стрибків (jump_height), відновлення псі-здоров'я (psy_health_restore_speed). Більш адекватне перерахування властивостей satiety_restore_speed (більше ніяких 5000%).
