#include "spirv_cross.h"
#include <utils/log.h>
#include <graphics/format/oish.h>
#include <graphics/shader.h>
#include <graphics/shaderstage.h>
#include <graphics/graphics.h>

#include <fstream>

using namespace oi;
using namespace oi ::wc;
using namespace oi::gc;
using namespace spirv_cross;

ShaderStageType pickExtension(String &s) {
	if (s == ".vert") return ShaderStageType::Vertex_shader;
	if (s == ".frag") return ShaderStageType::Fragment_shader;
	if (s == ".geom") return ShaderStageType::Geometry_shader;
	if (s == ".comp") return ShaderStageType::Compute_shader;
	Log::throwError<ShaderStageType, 0x0>("Couldn't pick a shader stage type from string; so extension is invalid");
	return ShaderStageType::Undefined;
}

//Vec2u; u32 buffer id, bool isInstanced
//This parses which buffer it belongs to;
//i0_m or i_m for example would be seen as a per instance variable in instance buffer 0
//a0_m or 0_m or a_m or m would be seen as an attribute in vertex buffer 0
//a1_m or 1_m would be an attribute in vertex buffer 1 (for example, a tangent could be in a different buffer, or per triangle material data)
//i1_m would be an attribute at instance buffer 1
//Modifies 'varName' into the name that isn't prefixed
Vec2u getBufferInfo(String &varName) {

	std::vector<String> split = varName.split("_");

	if (split.size() == 0) return Vec2u(0, 0);
	
	String &start = split[0];

	if (start.startsWith("i")) {

		if (start == "i") {
			varName = varName.cutBegin(2);	//remove i_
			return Vec2u(0, 1);
		}

		start = start.cutBegin(1);

		if (start.isUint()) {
			split.erase(split.begin());
			varName = String::combine(split, "_");	//remove i<x>_
			return Vec2u((u32)start.toLong(), 1U);
		}

	} 
	else if(start.startsWith("a")){

		if (start == "a") {
			varName = varName.cutBegin(2);	//remove a_
			return Vec2u(0, 0);
		}

		start = start.cutBegin(1);

		if (start.isUint()) {
			split.erase(split.begin());
			varName = String::combine(split, "_");	//remove a<x>_
			return Vec2u((u32)start.toLong(), 0U);
		}

	} 
	else if(start.isUint())
		return Vec2u((u32) start.toLong(), 0U);

	return Vec2u(0, 0);
}

TextureFormat getFormat(SPIRType type) {

	switch (type.basetype) {

	case SPIRType::BaseType::Half:
		return TextureFormat::R16f - (type.vecsize - 1);

	case SPIRType::BaseType::Float:
		return TextureFormat::R32f - (type.vecsize - 1);

	case SPIRType::BaseType::UInt:
		return TextureFormat::R32u - (type.vecsize - 1);

	case SPIRType::BaseType::Int:
		return TextureFormat::R32i - (type.vecsize - 1);

	case SPIRType::BaseType::UInt64:
		return TextureFormat::R64u - (type.vecsize - 1);

	case SPIRType::BaseType::Double:
		return TextureFormat::R64f - (type.vecsize - 1);

	case SPIRType::BaseType::Boolean:
	case SPIRType::BaseType::Char:
		return TextureFormat::R32u;

	default:
		return TextureFormat::Undefined;

	}


}

int main(int argc, char *argv[]) {

	String path, shaderName;
	std::vector<String> extensions;

	#ifdef _DEBUG
	if (argc == 1) {
		path = "D:/programming/repos/OEC/app/res/shaders/simple";
		shaderName = "simple";
		extensions = { ".vert", ".frag" };
		goto main;
	}
	#endif

	if (argc < 4) return (int) Log::error("Incorrect usage: oish_gen.exe <pathToShader> <shaderName> [shaderStage extensions]");

	path = argv[1];
	shaderName = argv[2];
	
	for (int i = 3; i < argc; ++i)
		extensions.push_back(argv[i]);

	main:

	ShaderInfo info;
	std::vector<ShaderStageInfo> stageInfo(extensions.size());

	std::vector<String> names = { shaderName };

	u32 i = 0;

	//Open the extensions' spirv and parse their data
	for (String &s : extensions) {

		ShaderStageType type = pickExtension(s);

		std::ifstream str((path + s + ".spv").toCString(), std::ios::binary);

		if (!str.good()) return (int) Log::error("Couldn't open that file");

		u32 length = (u32) str.rdbuf()->pubseekoff(0, std::ios_base::end);

		Buffer b(length);
		str.seekg(0, std::ios::beg);
		str.read((char*)b.addr(), b.size());

		str.close();

		stageInfo[i] = { b, type };
		++i;

		if (b.size() % 4 != 0)
			Log::throwError<VkNull, 0x0>("SPIRV bytecode incorrect");

		std::vector<uint32_t> bytecode((u32*)b.addr(), (u32*)(b.addr() + b.size()));
		Compiler comp(move(bytecode));

		ShaderResources res = comp.get_shader_resources();

		//Get the inputs
		if (type == ShaderStageType::Vertex_shader) {

			//The variables that we're going to be filling in
			std::vector<ShaderVBVar> &vars = info.var;
			vars.resize(res.stage_inputs.size());

			struct VBSection {

				ShaderVBSection section;
				u32 hash = u32_MAX;

				bool operator<(const VBSection &sec) const { return hash < sec.hash; }
			};

			//The sections that will be filled with the variables
			std::unordered_map<u32, VBSection> sections;

			u32 i = 0;

			//Convert the inputs from Resource (res.stage_inputs) to ShaderVBVar and ShaderVBSection
			for (Resource &r : res.stage_inputs) {

				Vec2u buf = getBufferInfo(vars[i].name = r.name);

				u32 hash = (buf.y << 31U) | (buf.x & 0x7FFFFFFFU);

				VBSection &section = sections[hash];
				ShaderVBSection &sec = section.section;

				if (section.hash == u32_MAX)
					section.hash = hash;

				sec.perInstance = (bool) buf.y;

				SPIRType type = comp.get_type_from_variable(r.id);
				vars[i].type = getFormat(type);
				u32 varSize = Graphics::getFormatSize(vars[i].type) * type.columns;
				vars[i].offset = sec.stride;
				vars[i].buffer = section.hash;

				sec.stride += varSize;

				vars[i].name = r.name;

				++i;
			}

			//Collapse into vector and sort them
			std::vector<std::pair<u32, VBSection>> elems(sections.begin(), sections.end());
			std::sort(elems.begin(), elems.end());

			i = 0;

			//Fix indices (convert instance to attributes + instanceId)
			for (auto &elem : elems) {
				info.section.push_back(elem.second.section);

				if ((elem.first & 0x80000000U) == 0)
					++i;
				else {
					elem.first = i + (elem.first & 0x7FFFFFFFU);
					for (auto &var : vars)
						if (var.buffer == elem.second.hash)
							var.buffer = elem.first;
				}
			}

			//Sort variables
			std::sort(vars.begin(), vars.end());
		}

		//Get the outputs
		if (type == ShaderStageType::Fragment_shader) {
			res.stage_outputs;
		}
	}

	return (int) 1;

}