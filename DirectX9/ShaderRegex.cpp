#include "ShaderRegex.h"
#include "globals.h" // For ShaderOverride FIXME: This should be in a separate header
#include "log.h"

#include <algorithm>
#include <iterator>

ShaderRegexGroups shader_regex_groups;

static void log_pcre2_error_nonl(int err, char *fmt, ...)
{
	PCRE2_UCHAR buf[120]; // doco says "120 code units is ample"
	va_list ap;

	pcre2_get_error_message(err, buf, sizeof(buf));

	va_start(ap, fmt);
	vLogInfo(fmt, ap);
	va_end(ap);

	LogInfo(": %s\n", buf);
}

static bool get_shader_model(std::string *asm_text, std::string *shader_model)
{
	size_t shader_model_pos;

	for (
		shader_model_pos = asm_text->find("\n");
		shader_model_pos != std::string::npos && (*asm_text)[shader_model_pos + 1] == '/';
		shader_model_pos = asm_text->find("\n", shader_model_pos + 1)
		) {
	}

	if (shader_model_pos == std::string::npos)
		return false;

	*shader_model = asm_text->substr(shader_model_pos + 1, asm_text->find("\n", shader_model_pos + 1) - shader_model_pos - 1);
	return true;
}

static bool find_dcl_end(std::string *asm_text, size_t *dcl_end_pos)
{
	// FIXME: Might be better to scan forwards

	*dcl_end_pos = asm_text->rfind("\ndcl_");
	*dcl_end_pos = asm_text->find("\n", *dcl_end_pos + 1);

	if (*dcl_end_pos == std::string::npos) {
		LogInfo("WARNING: Unable to locate end of shader declarations!\n");
		return false;
	}

	return true;
}

static bool insert_declarations(std::string *asm_text, ShaderRegexDeclarations *declarations)
{
	ShaderRegexDeclarations::iterator i;
	std::string insert_str;
	size_t dcl_end;
	bool patch = false;

	if (!find_dcl_end(asm_text, &dcl_end))
		return false;

	for (i = declarations->begin(); i != declarations->end(); i++) {
		insert_str = std::string("\n") + *i;

		if (asm_text->find(insert_str + std::string("\n")) != std::string::npos)
			continue;

		asm_text->insert(dcl_end, insert_str);
		dcl_end += insert_str.size();

		patch = true;
	}

	return patch;
}

static bool find_dcl_temps(std::string *asm_text, size_t *dcl_temps_pos)
{
	// Could use regex for this as well, but given we only need to find a
	// constant string it will be more efficient to just do this:
	*dcl_temps_pos = asm_text->find("\ndcl_temps ", 0);

	if (*dcl_temps_pos == std::string::npos)
		return false;

	return true;
}

static unsigned get_dcl_temps(std::string *asm_text)
{
	size_t dcl_temps;
	unsigned tmp_regs = 0;

	if (!find_dcl_temps(asm_text, &dcl_temps))
		return 0;

	tmp_regs = stoul(asm_text->substr(dcl_temps + 10, 4));
	LogInfo("Found dcl_temps %d\n", tmp_regs);

	return tmp_regs;
}

static bool update_dcl_temps(std::string *asm_text, size_t new_val)
{
	size_t dcl_temps, dcl_temps_end, dcl_end;
	std::string insert_str;

	if (find_dcl_temps(asm_text, &dcl_temps)) {
		dcl_temps += 11;
		dcl_temps_end = asm_text->find("\n", dcl_temps);
		LogInfo("Updating dcl_temps %Iu\n", new_val);
		asm_text->replace(dcl_temps, dcl_temps_end - dcl_temps, std::to_string(new_val));
		return true;
	}

	if (!find_dcl_end(asm_text, &dcl_end))
		return false;

	insert_str = std::string("\ndcl_temps ") + std::to_string(new_val);
	LogInfo("Inserting dcl_temps %Iu\n", new_val);
	asm_text->insert(dcl_end, insert_str);
	dcl_end += insert_str.size();

	return true;
}

ShaderRegexPattern::ShaderRegexPattern() :
	regex(NULL),
	do_replace(false)
{
}

ShaderRegexPattern::~ShaderRegexPattern()
{
	pcre2_code_free(regex);
}

bool ShaderRegexPattern::compile(std::string *pattern)
{
	uint32_t name_table_entry_size;
	uint32_t name_table_count;
	uint32_t i;
	PCRE2_SPTR name_table;
	PCRE2_SIZE err_off;
	int err;

	// CASELESS is for compatibility with d3dcompiler_46 & 47 without
	// having to always remember to account for the dcl_constantbuffer
	// differences:
	regex = pcre2_compile((PCRE2_SPTR)pattern->c_str(),
		pattern->length(), // or PCRE2_ZERO_TERMINATED
		PCRE2_CASELESS | PCRE2_MULTILINE,
		&err, &err_off, NULL);
	if (!regex) {
		log_pcre2_error_nonl(err, "  WARNING: PCRE2 regex compilation failed at offset %u", (unsigned)err_off);
		return false;
	}

	// TODO: Use callback to confirm that JIT does actually get used, as in
	// some cases pcre2 can fall back to using the slower interpreter
	pcre2_jit_compile(regex, 0);

	pcre2_pattern_info(regex, PCRE2_INFO_NAMECOUNT, &name_table_count);
	pcre2_pattern_info(regex, PCRE2_INFO_NAMEENTRYSIZE, &name_table_entry_size);
	pcre2_pattern_info(regex, PCRE2_INFO_NAMETABLE, &name_table);

	static_assert(PCRE2_CODE_UNIT_WIDTH == 8, "Need to fix name table parsing for non-8bit pcre2");
	for (i = 0; i < name_table_count; i++)
		named_capture_groups.insert(std::string((char*)(name_table + name_table_entry_size*i + 2)));

	return true;
}

bool ShaderRegexPattern::named_group_overlaps(ShaderRegexTemps &other_set)
{
	ShaderRegexTemps intersection;

	// C++ why you be so verbose?
	std::set_intersection(
		named_capture_groups.begin(),
		named_capture_groups.end(),
		other_set.begin(),
		other_set.end(),
		std::inserter(intersection, intersection.begin()));

	return intersection.size() != 0;
}

bool ShaderRegexPattern::matches(std::string *asm_text)
{
	pcre2_match_data *match_data = NULL;
	bool match = false;
	int rc;

	// TODO: Assign per-thread JIT stack if the default 32K turns out to be
	// insufficient. Can probably store this in the context, as that is
	// supposed to be per-thread, or use thread local storage.

	match_data = pcre2_match_data_create_from_pattern(regex, NULL);

	// TODO: Consider using pcre2_jit_match - doco claims 10% faster, but
	// has less sanity checks. TODO: Use callback to confirm JIT was used
	rc = pcre2_match(regex, (PCRE2_SPTR)asm_text->c_str(), asm_text->length(), 0, 0, match_data, NULL);
	if (rc == PCRE2_ERROR_NOMATCH)
		goto out_free;
	if (rc < 0) {
		log_pcre2_error_nonl(rc, "  WARNING: regex match error");
		goto out_free;
	}

	match = true;

out_free:
	pcre2_match_data_free(match_data);
	return match;
}

static void replacement_search_and_replace(std::string &str, std::string *search, std::string *replace)
{
	size_t pos;

	for (pos = str.find(*search); pos != std::string::npos; pos = str.find(*search, pos + 1)) {
		if (pos > 0 && (str[pos - 1] == '$' || str[pos - 1] == '\\'))
			continue;

		str.replace(pos, search->length(), *replace);
	}
}

static void substitute_temp_regs(std::string &replacement, ShaderRegexTemps *temp_regs, unsigned dcl_temps)
{
	ShaderRegexTemps::iterator i;
	unsigned tmp_reg = dcl_temps;
	std::string search_str, repl_str;

	for (i = temp_regs->begin(); i != temp_regs->end(); i++, tmp_reg++) {
		repl_str = std::string("r") + std::to_string(tmp_reg);

		search_str = std::string("$") + *i;
		replacement_search_and_replace(replacement, &search_str, &repl_str);

		search_str = std::string("${") + *i + std::string("}");
		replacement_search_and_replace(replacement, &search_str, &repl_str);
	}
}

bool ShaderRegexPattern::patch(std::string *asm_text, ShaderRegexTemps *temp_regs, unsigned dcl_temps)
{
	pcre2_match_data *match_data = NULL;
	PCRE2_SIZE est_size, output_size;
	std::string replace_copy;
	PCRE2_UCHAR *buf = NULL;
	bool patch = false;
	uint32_t options;
	int rc;

	static_assert(PCRE2_CODE_UNIT_WIDTH == 8, "Need to fix output buffer allocation for non-8bit pcre2");

	// We operate on a copy of the replace string so that future shaders
	// don't get our temporary register numbers:
	replace_copy = replace;
	substitute_temp_regs(replace_copy, temp_regs, dcl_temps);

	// TODO: Allow named capture groups from other patterns in the same
	// regex group to be substituted in, and provide some simple arithmetic
	// operators to e.g. allow a constant buffer byte offset to be divided
	// by 16 to get the constant buffer index and vice versa

	// At a minimum we want \n to be translated in the replace string,
	// which needs extended substitution processing to be enabled:
	options = PCRE2_SUBSTITUTE_EXTENDED;

	match_data = pcre2_match_data_create_from_pattern(regex, NULL);

	output_size = est_size = asm_text->length() + replace_copy.length() + 1024;
	buf = new PCRE2_UCHAR[output_size];
	rc = pcre2_substitute(regex,
		(PCRE2_SPTR)asm_text->c_str(), asm_text->length(), 0,
		options | PCRE2_SUBSTITUTE_OVERFLOW_LENGTH,
		match_data, NULL,
		(PCRE2_SPTR)replace_copy.c_str(), replace_copy.length(),
		buf, &output_size);

	if (rc == PCRE2_ERROR_NOMEMORY) {
		LogInfo("  NOTICE: regex replace requires a %u byte buffer\n", (unsigned)output_size);
		LogInfo("  NOTICE: We underestimated by %u bytes and have to start over\n", (unsigned)(output_size - est_size));
		LogInfo("  NOTICE: What kind of crazy are you doing to get down this code path?\n");
		LogInfo("  NOTICE: You didn't inject a matrix inverse or two in assembly did you?\n");
		LogInfo("  NOTICE: Once more, with passion!\n");

		delete[] buf;
		buf = new PCRE2_UCHAR[output_size];

		rc = pcre2_substitute(regex,
			(PCRE2_SPTR)asm_text->c_str(), asm_text->length(), 0,
			options, // No PCRE2_SUBSTITUTE_OVERFLOW_LENGTH this time
			match_data, NULL,
			(PCRE2_SPTR)replace_copy.c_str(), replace_copy.length(),
			buf, &output_size);
	}

	if (rc == 0)
		goto out_free;
	if (rc < 0) {
		log_pcre2_error_nonl(rc, "  WARNING: regex replace error");
		goto out_free;
	}

	*asm_text = (char*)buf;
	patch = true;

out_free:
	pcre2_match_data_free(match_data);
	delete[] buf;

	return patch;
}

void ShaderRegexGroup::apply_regex_patterns(std::string *asm_text, bool *match, bool *patch)
{
	ShaderRegexPatterns::iterator i;
	ShaderRegexPattern *pattern;
	unsigned dcl_temps = 0;

	// Match defaults to true so that if there are no patterns we can still
	// apply the command list. Patch defaults to false because we don't
	// want to waste time re-assembling the shader if we didn't change it.
	*match = true;
	*patch = false;

	if (!temp_regs.empty())
		dcl_temps = get_dcl_temps(asm_text);

	for (i = patterns.begin(); i != patterns.end(); i++) {
		pattern = &i->second;

		if (pattern->do_replace)
			*match = *patch = pattern->patch(asm_text, &temp_regs, dcl_temps);
		else
			*match = pattern->matches(asm_text);

		if (!*match) {
			*patch = false;
			return;
		}
	}

	// Only update dcl_temps if we are patching:
	if (*patch && !temp_regs.empty())
		*patch = update_dcl_temps(asm_text, dcl_temps + temp_regs.size());

	// But we can update declarations even if we aren't doing a regex
	// replace in some cases, so long as the patterns all matched (e.g.
	// globally disable the driver stereo cb):
	if (!declarations.empty())
		*patch = insert_declarations(asm_text, &declarations) || *patch;
}

void ShaderRegexGroup::link_command_lists(UINT64 shader_hash)
{
	ShaderOverride *shader_override = NULL;
	wchar_t buf[32];
	wstring ini_section, ini_line;

	// Only link the command lists if we have something in ours to link in,
	// because this will create ShaderOverride sections for shaders that
	// don't already have one, adding more work in the draw calls.

	if (command_list.commands.empty() && post_command_list.commands.empty())
		return;

	swprintf_s(buf, ARRAYSIZE(buf), L".Match=%016llx", shader_hash);
	ini_section = command_list.ini_section + buf;

	shader_override = &G->mShaderOverrideMap[shader_hash];
	if (shader_override->command_list.ini_section.empty()) {
		shader_override->command_list.ini_section = ini_section;
		shader_override->post_command_list.ini_section = ini_section;
		shader_override->post_command_list.post = true;
	}

	ini_line = L"[" + shader_override->command_list.ini_section + L"] run = " + command_list.ini_section;

	if (!command_list.commands.empty())
		LinkCommandLists(&shader_override->command_list, &command_list, &ini_line);

	if (!post_command_list.commands.empty())
		LinkCommandLists(&shader_override->post_command_list, &post_command_list, &ini_line);
}

bool apply_shader_regex_groups(std::string *asm_text, std::string *shader_model, UINT64 hash, std::wstring *tagline)
{
	ShaderRegexGroups::iterator i;
	ShaderRegexGroup *group;
	bool patched = false;
	bool match, patch;

	if (*shader_model == std::string("bin")) {
		// This will update the data structure, because we may as well
		// - it will save effort if we have to redo this again later.
		if (!get_shader_model(asm_text, shader_model))
			return false;
	}

	for (i = shader_regex_groups.begin(); i != shader_regex_groups.end(); i++) {
		group = &i->second;

		// FIXME: Don't even disassemble if the shader model isn't in
		// any of the regex groups and we aren't applying any other
		// forms of deferred patches
		if (!group->shader_models.count(*shader_model))
			continue;

		group->apply_regex_patterns(asm_text, &match, &patch);
		if (!match)
			continue;

		LogInfo("ShaderRegex: %s %016I64x matches [%S]\n", shader_model->c_str(), hash, group->ini_section.c_str());
		patched = patched || patch;

		if (patch && tagline)
			tagline->append(std::wstring(L"[") + group->ini_section + std::wstring(L"]"));

		group->link_command_lists(hash);
	}

	return patched;
}
