Форк рушія ТЧ 1.0007 від xrModder

Ключові відмінності на 03.10.2026:
- Інерція HUD у прицілюванні + можливість налаштування у конфігу конкретної зброї (hud_aim_inertion = true/false, hud_aim_inertion_strength = ...).
- Розкачка HUD як у старих збірках ТЧ + можливість налаштування (нові параметри для [bobbing_effector] у effectors.ltx: hud_bobbing_speed, hud_bobbing_amplitude, hud_bobbing_vertical).
- Рух HUD зброї при стрибках і приземленні + можливість налаштування (нові параметри для [bobbing_effector] у effectors.ltx: hud_jump_down, hud_jump_up, hud_jump_duration, hud_jump_pitch_down, hud_jump_pitch_up, hud_landing_duration, hud_landing_down, hud_landing_pitch_down).
- Можливість скриптово змінювати ім'я та іконку актора (actor:set_actor_name, actor:set_actor_icon) + винесено в xr_effects: change_actor_name, change_actor_icon).
- Відображення текстури-заглушки замість вильоту по "can`t find texture" + так само зроблено і для потенційно поламаних dds.
- Можливість встановити круглий приціл, як у білдах (консольна команда hud_cursor_weapon).
- Нові гарячі клавіші: на швидке використання їжі, антираду, на дію "Взяти все" і дію "Розрядити зброю в інвентарі".
