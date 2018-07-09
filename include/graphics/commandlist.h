#pragma once

#include <types/vector.h>
#include "graphics/gl/generic.h"
#include "graphics/graphicsobject.h"

namespace oi {

	namespace gc {

		class Graphics;
		class RenderTarget;
		class Pipeline;
		class GBuffer;
		class MeshBuffer;
		class Mesh;
		class DrawList;

		struct CommandListInfo { };

		struct RenderTargetClear {

			Vec4d colorClear;
			f32 depthClear;
			u32 stencilClear;

			RenderTargetClear(Vec4d color = {}, f32 depth = 1.f, u32 stencil = 0U) : colorClear(color), depthClear(depth), stencilClear(stencil) {}

		};

		class CommandList : public GraphicsObject {

			friend class Graphics;
			friend class Texture;

		public:

			void begin();
			void begin(RenderTarget *target, RenderTargetClear clear = {});

			void end();
			void end(RenderTarget *target);

			void bind(Pipeline *pipeline);
			bool bind(MeshBuffer *meshBuffer);
			void draw(Mesh *mesh, u32 instances = 1U);
			void draw(DrawList *drawList);

			CommandListExt &getExtension();

		protected:

			bool bind(std::vector<GBuffer*> vertices, GBuffer *indices = nullptr);
			void draw(u32 vertices, u32 instances = 1U, u32 startVertex = 0U, u32 startInstance = 0U);
			void drawIndexed(u32 indices, u32 instances = 1U, u32 startIndex = 0U, u32 startVertex = 0U, u32 startInstance = 0U);

			~CommandList();
			CommandList(CommandListInfo info);
			bool init();

			void flush();

		private:

			CommandListInfo info;
			CommandListExt ext;

		};

	}

}