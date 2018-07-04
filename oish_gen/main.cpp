#include "spirv_cross.h"
#include <utils/log.h>
#include <graphics/format/oish.h>
#include <graphics/shader.h>
#include <graphics/shaderstage.h>
#include <graphics/graphics.h>

#include <fstream>

#pragma comment(lib, "Xinput.lib")

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

ShaderVBSection &insertSection(Vec2u buf, std::vector<ShaderVBSection> &sec) {

	bool perInstance = (bool) buf.y;

	if (sec.size() <= buf.x)
		sec.resize(buf.x + 1U);

	ShaderVBSection &sect = sec[buf.x];

	if (sect.stride == 0)
		sect.perInstance = perInstance;
	
	if (sect.perInstance != perInstance)
		Log::throwError<ShaderVBSection, 0x0>("Couldn't be inserted; the type of a buffer section can't be changed to or from instance to or from attribute");

	return sect;
}

void fillStruct(Compiler &comp, u32 id, ShaderBufferInfo &info, ShaderBufferObject *var) {

	auto &type = comp.get_type(id);
	
	for (u32 i = 0; i < (u32)type.member_types.size(); ++i) {

		ShaderBufferObject obj;

		const SPIRType &mem = comp.get_type(type.member_types[i]);

		obj.offset = (u32) comp.type_struct_member_offset(type, i);
		obj.name = comp.get_member_name(id, i);

		u32 varId = var == &info.self ? 0U : (u32)(var - info.elements.data()) + 1U;

		if (mem.basetype == SPIRType::Struct) {

			u32 size = (u32)comp.get_declared_struct_member_size(mem, i);

			obj.length = size;
			obj.arraySize = mem.array.size() == 0 ? 1U : (u32) mem.array[0];
			obj.format = TextureFormat::Undefined;

			u32 objoff = (u32) info.elements.size();

			info.push(obj, *var);
			var = varId == 0 ? &info.self : info.elements.data() + varId - 1U;

			fillStruct(comp, type.member_types[i], info, info.elements.data() + objoff);

		} else {

			obj.format = getFormat(mem);
			obj.arraySize = mem.columns;
			obj.length = Graphics::getFormatSize(obj.format);

			info.push(obj, *var);
			var = varId == 0 ? &info.self : info.elements.data() + varId - 1U;
		}
	}

}

int main(int argc, char *argv[]) {

	String path, shaderName;
	std::vector<String> extensions;

	if (argc < 4) return (int) Log::error("Incorrect usage: oish_gen.exe <pathToShader> <shaderName> [shaderStage extensions]");

	path = argv[1];
	shaderName = argv[2];
	
	for (int i = 3; i < argc; ++i)
		extensions.push_back(argv[i]);

	ShaderInfo info;
	info.path = shaderName;

	std::vector<ShaderStageInfo> &stageInfo = info.stages;
	stageInfo.resize(extensions.size());

	std::vector<String> names = { shaderName };

	u32 j = 0, k = 0;

	//Open the extensions' spirv and parse their data
	for (String &s : extensions) {

		ShaderStageType type = pickExtension(s);

		//Load debug spirv (with all variable names)

		std::ifstream str((path + s + ".spv").toCString(), std::ios::binary);

		if (!str.good()) return (int)Log::error("Couldn't open that file");

		u32 length = (u32)str.rdbuf()->pubseekoff(0, std::ios_base::end);

		Buffer b(length);
		str.seekg(0, std::ios::beg);
		str.read((char*)b.addr(), b.size());

		str.close();

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

			u32 i = 0;

			std::vector<ShaderVBSection> &sections = info.section;

			//Convert the inputs from Resource (res.stage_inputs) to ShaderVBVar and ShaderVBSection
			for (Resource &r : res.stage_inputs) {

				Vec2u buf = getBufferInfo(vars[i].name = r.name);

				ShaderVBSection &section = insertSection(buf, sections);

				SPIRType type = comp.get_type_from_variable(r.id);
				vars[i].type = getFormat(type);
				u32 varSize = Graphics::getFormatSize(vars[i].type) * type.columns;
				vars[i].offset = section.stride;
				vars[i].buffer = buf.x;

				section.stride += varSize;

				vars[i].name = r.name;

				++i;
			}
		}

		//Get the outputs
		if (type == ShaderStageType::Fragment_shader) {

			info.output.resize(res.stage_outputs.size());

			u32 i = 0;
			for (Resource &r : res.stage_outputs) {
				info.output[i] = ShaderOutput(getFormat(comp.get_type_from_variable(r.id)), r.name, comp.get_decoration(r.id, spv::DecorationLocation));
				++i;
			}

		}

		//Get the registers

		std::vector<Resource> buf = res.uniform_buffers;
		buf.insert(buf.end(), res.storage_buffers.begin(), res.storage_buffers.end());

		ShaderRegisterAccess stageAccess = type.getName().replace("_shader", "");

		u32 i = 0;
		for (Resource &r : buf) {

			u32 binding = comp.get_decoration(r.id, spv::DecorationBinding);
			
			bool isUBO = i < res.uniform_buffers.size();

			ShaderRegisterType stype = !isUBO ? 2U : 1U;

			if(info.registers.size() <= binding)
				info.registers.resize(binding + 1U);

			ShaderRegister &reg = info.registers[binding];

			if (reg.name == "") 
				reg = ShaderRegister(stype, stageAccess, r.name);
			else {

				reg.access = reg.access.getValue() | stageAccess.getValue();

				if (reg.access == ShaderRegisterAccess::Undefined)
					return (int)Log::error("Invalid register access");
			}

			String name = String(r.name).replaceLast("_ext", "");

			info.bufferIds[k] = name;
			ShaderBufferInfo &dat = info.buffer[name];

			const SPIRType &btype = comp.get_type(r.base_type_id);

			dat.size = (u32) comp.get_declared_struct_size(btype);
			dat.allocate = String(r.name).endsWithIgnoreCase("_ext");
			dat.type = reg.type;

			dat.self.arraySize = 1U;
			dat.self.length = dat.size;
			dat.self.format = TextureFormat::Undefined;
			dat.self.name = name;
			dat.self.offset = 0U;
			dat.self.parent = nullptr;

			fillStruct(comp, r.base_type_id, dat, &dat.self);

			++i;
			++k;
		}

		for (Resource &r : res.separate_images) {

			u32 binding = comp.get_decoration(r.id, spv::DecorationBinding);
			bool isWriteable = comp.get_decoration(r.id, spv::DecorationNonWritable) == 0U;

			if (info.registers.size() <= binding)
				info.registers.resize(binding + 1U);

			ShaderRegister &reg = info.registers[binding];

			if(reg.name == "")
				reg = ShaderRegister(isWriteable ? ShaderRegisterType::Image : ShaderRegisterType::Texture2D, stageAccess, r.name);
			else {

				reg.access = reg.access.getValue() | stageAccess.getValue();

				if (reg.access == ShaderRegisterAccess::Undefined)
					return (int)Log::error("Invalid register access");
			}

		}

		for (Resource &r : res.separate_samplers) {

			u32 binding = comp.get_decoration(r.id, spv::DecorationBinding);

			if (info.registers.size() <= binding)
				info.registers.resize(binding + 1U);

			ShaderRegister &reg = info.registers[binding];

			if (reg.name == "")
				reg = ShaderRegister(ShaderRegisterType::Sampler, stageAccess, r.name);
			else {

				reg.access = reg.access.getValue() | stageAccess.getValue();

				if (reg.access == ShaderRegisterAccess::Undefined)
					return (int)Log::error("Invalid register access");
			}

		}

		b.deconstruct();

		//Load optimized spirv

		std::ifstream ospv((path + s + ".ospv").toCString(), std::ios::binary);

		if (!ospv.good()) return (int) Log::error("Couldn't open that file");

		length = (u32) ospv.rdbuf()->pubseekoff(0, std::ios_base::end);

		b = Buffer(length);
		ospv.seekg(0, std::ios::beg);
		ospv.read((char*)b.addr(), b.size());
		stageInfo[j] = { b, type };
		ospv.close();

		++j;
	}

	Buffer b = oiSH::write(oiSH::convert(info));

	std::ofstream oish((path + ".oiSH").toCString(), std::ios::binary);

	if (!oish.good()) return (int)Log::error("Couldn't open that file");
	
	oish.write((char*)b.addr(), b.size());
	oish.close();

	b.deconstruct();

	Log::println(String("Successfully converted to ") + path + ".oiSH");

	return 1U;
}