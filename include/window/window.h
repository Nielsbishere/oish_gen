#pragma once
#include "windowinfo.h"
#include "input/inputmanager.h"
#include "platforms/generic.h"

namespace oi {

	namespace wc {

		class WindowInterface;
		class WindowManager;

		class Window {

			friend class WindowInfo;
			friend class WindowManager;
			friend class InputHandler;
			friend struct WindowExt;

		public:

			void setInterface(WindowInterface *wi);

			WindowInfo &getInfo();
			InputHandler &getInputHandler();
			InputManager &getInputManager();
			WindowInterface *getInterface();
			WindowManager *getParent();

			WindowExt &getExtension();
			
			void *getSurfaceData();
			u32 getSurfaceSize();

			bool hasPreviousFrame();

		protected:

			void update();
			void updatePlatform();
			void init();
			void initPlatform();
			void finalize();
			void destroyPlatform();

			Window(WindowManager *parent, WindowInfo info);
			~Window();

		private:

			WindowManager *parent;
			WindowInfo info;
			InputHandler inputHandler;
			InputManager inputManager;
			WindowInterface *wi = nullptr;
			bool initialized = false, hasPrevFrame = false;
			u32 finalizeCount = 0;

			flp lastTick = (flp) 0;

			WindowExt ext;

		};
	}

}