#include "stdafx.h"
#include "string_table.h"

#include "ui/xrUIXmlParser.h"
#include "xr_level_controller.h"

STRING_TABLE_DATA* CStringTable::pData = NULL;
BOOL CStringTable::m_bWriteErrorsToLog = FALSE;

CStringTable::CStringTable	()
{
	Init();
}

void CStringTable::Destroy	()
{
	xr_delete(pData);
}


void CStringTable::Init()
{
	if (NULL != pData)
		return;

	pData = xr_new<STRING_TABLE_DATA>();

	// Выбираем язык из localization.ltx.
	pData->m_sLanguage =
		pSettings->r_string("string_table", "language");

	// Формируем путь к каталогу и маску XML-файлов.
	string_path mask;
	string_path prefix;

	strconcat(
		sizeof(mask),
		mask,
		STRING_TABLE_PATH,
		"\\",
		*(pData->m_sLanguage),
		"\\*.xml"
	);

	strconcat(
		sizeof(prefix),
		prefix,
		STRING_TABLE_PATH,
		"\\",
		*(pData->m_sLanguage),
		"\\"
	);

	// Имена файлов в файловой системе движка представлены
	// в нижнем регистре.
	xr_strlwr(mask);
	xr_strlwr(prefix);

	// Автоматически находим XML-файлы выбранного языка.
	FS_FileSet files;

	FS.file_list(
		files,
		"$game_config$",
		FS_ListFiles,
		mask
	);

	if (files.empty())
	{
		Debug.fatal(
			DEBUG_INFO,
			"String table XML files not found for language %s",
			*(pData->m_sLanguage)
		);
	}

	Msg(
		"! [string_table] Found %u XML files for language '%s'",
		(u32)files.size(),
		*(pData->m_sLanguage)
	);

	const u32 prefix_len = (u32)xr_strlen(prefix);

	for (FS_FileSetIt it = files.begin(); it != files.end(); ++it)
	{
		LPCSTR full_name = it->name.c_str();

		// Проверяем, что файл относится к выбранному языку.
		if (strncmp(full_name, prefix, prefix_len) != 0)
			continue;

		// Получаем имя файла без пути.
		LPCSTR base_name = full_name + prefix_len;

		// Не загружаем файлы из вложенных каталогов.
		if (strchr(base_name, '\\') || strchr(base_name, '/'))
			continue;

		// CStringTable::Load ожидает имя без расширения .xml.
		string_path xml_file;
		strcpy(xml_file, base_name);

		LPSTR ext = strrchr(xml_file, '.');

		if (!ext || stricmp(ext, ".xml") != 0)
			continue;

		*ext = 0;

		// Используем оригинальный механизм загрузки переводов.
		Load(xml_file);
	}
}

void CStringTable::Load	(LPCSTR xml_file)
{
	CUIXml						uiXml;
	string_path					xml_file_full;
	strconcat					(sizeof(xml_file_full),xml_file_full, xml_file, ".xml");
	string_path					_s;
	strconcat					(sizeof(_s),_s, STRING_TABLE_PATH, "\\", *(pData->m_sLanguage) );

	bool xml_result				= uiXml.Init(CONFIG_PATH, _s, xml_file_full);
	if(!xml_result)
		Debug.fatal(DEBUG_INFO,"string table xml file not found %s, for language %s", xml_file_full, *(pData->m_sLanguage));

	//общий список всех записей таблицы в файле
	int string_num = uiXml.GetNodesNum		(uiXml.GetRoot(), "string");

	for(int i=0; i<string_num; ++i)
	{
		LPCSTR string_name = uiXml.ReadAttrib(uiXml.GetRoot(), "string", i, "id", NULL);

		VERIFY3					(pData->m_StringTable.find(string_name) == pData->m_StringTable.end(), "duplicate string table id", string_name);

		LPCSTR string_text		= uiXml.Read(uiXml.GetRoot(), "string:text", i,  NULL);

		if(m_bWriteErrorsToLog && string_text)
			Msg("[string table] '%s' no translation in '%s'", string_name, *(pData->m_sLanguage));
		
		VERIFY3						(string_text, "string table entry does not has a text", string_name);
		
		STRING_VALUE str_val		= ParseLine(string_text, string_name, true);
		
		pData->m_StringTable[string_name] = str_val;
	}
}
void CStringTable::ReparseKeyBindings()
{
	if(!pData)					return;
	STRING_TABLE_MAP_IT it		= pData->m_string_key_binding.begin();
	STRING_TABLE_MAP_IT it_e	= pData->m_string_key_binding.end();

	for(;it!=it_e;++it)
	{
		pData->m_StringTable[it->first]			= ParseLine(*it->second, *it->first, false);
	}
}


STRING_VALUE CStringTable::ParseLine(LPCSTR str, LPCSTR skey, bool bFirst)
{
//	LPCSTR str = "1 $$action_left$$ 2 $$action_right$$ 3 $$action_left$$ 4";
	xr_string			res;
	int k = 0;
	const char*			b;
	#define ACTION_STR "$$ACTION_"

//.	int LEN				= (int)xr_strlen(ACTION_STR);
	#define LEN			9

	string256				buff;
	string256				srcbuff;
	bool	b_hit			= false;

	while( (b = strstr( str+k,ACTION_STR)) !=0 )
	{
		buff[0]				= 0;
		srcbuff[0]			= 0;
		res.append			(str+k, b-str-k);
		const char* e		= strstr( b+LEN,"$$" );

		int len				= (int)(e-b-LEN);

		strncpy				(srcbuff,b+LEN, len);
		srcbuff[len]		= 0;
		GetActionAllBinding	(srcbuff, buff, sizeof(buff) );
		res.append			(buff, xr_strlen(buff) );

		k					= (int)(b-str);
		k					+= len;
		k					+= LEN;
		k					+= 2;
		b_hit				= true;
	};

	if(k<(int)xr_strlen(str)){
		res.append(str+k);
	}

	if(b_hit&&bFirst) pData->m_string_key_binding[skey] = str;

	return STRING_VALUE(res.c_str());
}

STRING_VALUE CStringTable::translate (const STRING_ID& str_id) const
{
	VERIFY					(pData);

	STRING_VALUE res =  pData->m_StringTable[str_id];

	if(!res)
	{
		if(m_bWriteErrorsToLog && *str_id != NULL && xr_strlen(*str_id)>0)
			Msg("[string table] '%s' has no entry", *str_id);
		return str_id;
	}
	return					pData->m_StringTable[str_id];
}
