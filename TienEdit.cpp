#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#include "TienEdit.h"
#include "resource.h"

#include <Windows.h>
#include <direct.h>
#include <algorithm>
#include <fstream>

#include <VrLib/gl/shader.h>
#include <vrlib/gl/FBO.h>
#include <vrlib/gl/Vertex.h>
#include <vrlib/Log.h>
#include <VrLib/Kernel.h>
#include <VrLib/math/aabb.h>
#include <VrLib/util.h>

#include <math.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>


#include <vrlib/tien/components/Camera.h>
#include <vrlib/tien/components/Transform.h>
#include <vrlib/tien/components/Light.h>
#include <vrlib/tien/components/DynamicSkyBox.h>
#include <vrlib/tien/components/ModelRenderer.h>
#include <vrlib/tien/components/MeshRenderer.h>

#include "actions/Action.h"
#include "menu/MenuOverlay.h"
#include "menu/Menu.h"
#include "wm/SplitPanel.h"
#include "wm/RenderComponent.h"
#include "wm/Tree.h"
#include "wm/Panel.h"
#include "wm/Label.h"
#include "wm/Image.h"

#include "actions/SelectionChangeAction.h"

#include "EditorBuilderGui.h"


TienEdit::TienEdit(const std::string &fileName) : NormalApp("TiEn scene Editor")
{
}


TienEdit::~TienEdit()
{
}






void TienEdit::init()
{
	HWND hWnd = GetActiveWindow();
	HINSTANCE hInstance = GetModuleHandle(NULL);
	HICON icon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	SendMessage(hWnd, WM_SETICON, ICON_SMALL,	(LPARAM)icon);
	SendMessage(hWnd, WM_SETICON, ICON_BIG,		(LPARAM)icon);

	kernel = vrlib::Kernel::getInstance();

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	tien.init();
	menuOverlay.init();
	menuOverlay.loadMenu("data/TiEnEdit/menu.json");

	mainPanel = new SplitPanel();


	class TienNodeTree : public Tree::TreeLoader
	{
		TienEdit* edit;
	public:
		TienNodeTree(TienEdit* edit)
		{
			this->edit = edit;
		}
		virtual std::string getName(void* data)
		{
			if (data == nullptr)
				return "root";
			auto n = static_cast<vrlib::tien::Node*>(data);
			return n->name;
		}
		virtual int getChildCount(void* data)
		{
			if (data == nullptr)
				return edit->tien.scene.getChildren().size();
			auto n = static_cast<vrlib::tien::Node*>(data);
			return n->getChildren().size();
		}
		virtual void* getChild(void* data, int index)
		{
			if (data == nullptr)
				return edit->tien.scene.getChild(index);
			else
			{
				auto n = static_cast<vrlib::tien::Node*>(data);
				return n->getChild(index);
			}
		}
		virtual int getIcon(void* data)
		{
			auto n = static_cast<vrlib::tien::Node*>(data);
			if (n->getComponent<vrlib::tien::components::ModelRenderer>())
				return 2;
			if (n->getComponent<vrlib::tien::components::MeshRenderer>())
				return 2;
			if (n->getComponent<vrlib::tien::components::Camera>())
				return 4;
			if (n->getComponent<vrlib::tien::components::Light>())
				return 5;

			return 3;
		}
	};

	modelBrowsePanel = nullptr;

	objectTree = new Tree();
	objectTree->loader = new TienNodeTree(this);
	mainPanel->addPanel(objectTree);
	
	mainPanel->addPanel(renderPanel = new RenderComponent());
	Panel* propertiesPanel = new Panel();
	mainPanel->addPanel(propertiesPanel);


	GuiEditor* editorBuilder = new GuiEditor(this, propertiesPanel);

	objectTree->rightClickItem = [this]()
	{
		if (objectTree->selectedItem)
		{
			vrlib::json::Value popupMenu = vrlib::json::readJson(std::ifstream("data/TiEnEdit/nodemenu.json"));
			menuOverlay.popupMenus.push_back(std::pair<glm::vec2, Menu*>(mouseState.pos, new Menu(popupMenu)));
		}
		else
		{
			vrlib::json::Value popupMenu = vrlib::json::readJson(std::ifstream("data/TiEnEdit/newnodemenu.json"));
			Menu* menu = new Menu(popupMenu);
			menuOverlay.popupMenus.push_back(std::pair<glm::vec2, Menu*>(mouseState.pos, menu));
			menu->setAction("new model", [this]()
			{
				browseCallback = [this](const std::string &fileName)
				{
					vrlib::tien::Node* n = new vrlib::tien::Node("new node", &tien.scene);
					n->addComponent(new vrlib::tien::components::Transform());
					n->addComponent(new vrlib::tien::components::ModelRenderer(fileName));
					objectTree->update();
				};
				showBrowsePanel();
			});
			
		}
	};
	objectTree->selectItem = [this, propertiesPanel, editorBuilder]()
	{
		vrlib::tien::Node* node = static_cast<vrlib::tien::Node*>(objectTree->selectedItem);

		for (auto c : propertiesPanel->components)
			delete c;
		propertiesPanel->components.clear();
		editorBuilder->reset();

		editorBuilder->addTitle("Node:");
		
		editorBuilder->beginGroup("Name");
		editorBuilder->addTextBox(node->name, [node, this](const std::string &newValue) { node->name = newValue; objectTree->update(); });
		editorBuilder->endGroup();

		editorBuilder->beginGroup("GUID");
		editorBuilder->addTextBox(node->guid, [node](const std::string &newValue) { node->guid = newValue; });
		editorBuilder->endGroup();

		editorBuilder->beginGroup("Parent");
		editorBuilder->addTextBox(node->parent ? node->parent->name : "-", [node](const std::string &newValue) {  });
		editorBuilder->endGroup();

		std::vector<vrlib::tien::Component*> components = node->getComponents();
		for (auto c : components)
			c->buildEditor(editorBuilder);
		propertiesPanel->onReposition(mainPanel);

		editorBuilder->addTitle("");
		editorBuilder->beginGroup("Add Component");
		editorBuilder->addButton("Add", [node]() {});
		editorBuilder->endGroup();

	};

	mainPanel->position = glm::ivec2(0, 25+36);
	mainPanel->size = glm::ivec2(kernel->getWindowWidth(), kernel->getWindowHeight());
	mainPanel->sizes[0] = 300;
	mainPanel->sizes[2] = 300;
	mainPanel->sizes[1] = mainPanel->size.x - 600;
	mainPanel->onReposition(nullptr);

	vrlib::tien::Node* sunlight;
	{
		vrlib::tien::Node* n = new vrlib::tien::Node("Sunlight", &tien.scene);
		n->addComponent(new vrlib::tien::components::Transform(glm::vec3(0, 1, 1)));
		vrlib::tien::components::Light* light = new vrlib::tien::components::Light();
		light->color = glm::vec4(1, 1, 0.8627f, 1);
		light->intensity = 20.0f;
		light->type = vrlib::tien::components::Light::Type::directional;
		light->shadow = vrlib::tien::components::Light::Shadow::shadowmap;
		n->addComponent(light);
		sunlight = n;
	}

	{
		vrlib::tien::Node* n = new vrlib::tien::Node("Camera", &tien.scene);
		n->addComponent(new vrlib::tien::components::Transform(glm::vec3(0,0, 0)));
		n->addComponent(new vrlib::tien::components::Camera());
		n->addComponent(new vrlib::tien::components::DynamicSkyBox());
		n->getComponent<vrlib::tien::components::DynamicSkyBox>()->light = sunlight;
		tien.scene.cameraNode = n;
	}

	{
		vrlib::tien::Node* n = new vrlib::tien::Node("Model", &tien.scene);
		n->addComponent(new vrlib::tien::components::Transform(glm::vec3(0, 0, 0), glm::quat(), glm::vec3(1,1,1)));
		n->addComponent(new vrlib::tien::components::ModelRenderer("data/Models/vangogh/Enter a title.obj"));

		//n->addComponent(new vrlib::tien::components::ModelRenderer("data/tientest/models/WoodenBox02.obj"));
//		n->addComponent(new vrlib::tien::components::Transform(glm::vec3(0, 0, 0), glm::quat(), glm::vec3(0.01f, 0.01f, 0.01f)));
//		n->addComponent(new vrlib::tien::components::ModelRenderer("data/tientest/models/crytek-sponza/sponza.obj"));
		n->getComponent<vrlib::tien::components::ModelRenderer>()->castShadow = false;
	}

	focussedComponent = renderPanel;

	objectTree->update();


	cameraPos = glm::vec3(0, 1.8f, 8.0f);

	tien.start();

}

void TienEdit::preFrame(double frameTime, double totalTime)
{
	menuOverlay.setWindowSize(kernel->getWindowSize());
	menuOverlay.hover();

	mainPanel->size = glm::ivec2(kernel->getWindowWidth(), kernel->getWindowHeight() - 25 - 36);
	mainPanel->sizes[1] = mainPanel->size.x - 600;
	mainPanel->onReposition(nullptr); //TODO: don't do this every frame ._.;


	if (focussedComponent == renderPanel || mouseState.middle)
	{
		glm::vec3 cameraMovement(0, 0, 0);
		if (KeyboardDeviceDriver::isPressed(KeyboardDeviceDriver::KEY_W))		cameraMovement.z = -1;
		if (KeyboardDeviceDriver::isPressed(KeyboardDeviceDriver::KEY_S))		cameraMovement.z = 1;
		if (KeyboardDeviceDriver::isPressed(KeyboardDeviceDriver::KEY_A))		cameraMovement.x = -1;
		if (KeyboardDeviceDriver::isPressed(KeyboardDeviceDriver::KEY_D))		cameraMovement.x = 1;
		if (KeyboardDeviceDriver::isPressed(KeyboardDeviceDriver::KEY_Q))		cameraMovement.y = 1;
		if (KeyboardDeviceDriver::isPressed(KeyboardDeviceDriver::KEY_Z))		cameraMovement.y = -1;

		cameraMovement *= frameTime / 100.0f;
		cameraPos += cameraMovement * cameraRot;
	}


	if (mouseState.middle)
	{
		cameraRot = cameraRot * glm::quat(glm::vec3(0, .01f * (mouseState.pos.x - lastMouseState.pos.x), 0));
		cameraRot = glm::quat(glm::vec3(.01f * (mouseState.pos.y - lastMouseState.pos.y), 0, 0)) * cameraRot;
	}


	if (mainPanel->components[1] == renderPanel)
		tien.update((float)(frameTime / 1000.0f));


	lastMouseState = mouseState;
}


void TienEdit::draw()
{
	float aspect = (float)kernel->getWindowWidth() / kernel->getWindowHeight();


	glViewport(0, 0, kernel->getWindowWidth(), kernel->getWindowHeight());
	menuOverlay.drawInit();
	menuOverlay.drawRootMenu();
	mainPanel->draw(&menuOverlay);
	menuOverlay.drawPopups();

	
	if (mainPanel->components[1] == renderPanel)
	{
		glm::mat4 cameraMat = glm::translate(glm::toMat4(cameraRot), -cameraPos);
		glm::mat4 projectionMatrix = glm::perspective(70.0f, renderPanel->size.x / (float)renderPanel->size.y, 0.01f, 500.0f);
		glm::mat4 modelViewMatrix = cameraMat;
		glViewport(renderPanel->position.x, kernel->getWindowHeight() - renderPanel->position.y - renderPanel->size.y, renderPanel->size.x, renderPanel->size.y);
		tien.render(projectionMatrix, modelViewMatrix);

		glMatrixMode(GL_PROJECTION);
		glLoadMatrixf(glm::value_ptr(projectionMatrix));
		glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf(glm::value_ptr(modelViewMatrix));
		glUseProgram(0);
		glDisable(GL_TEXTURE_2D);
		glColor4f(1, 0, 0, 1);
		glDisable(GL_BLEND);
		glBegin(GL_LINES);
		glVertex3f(ray.mOrigin.x, ray.mOrigin.y, ray.mOrigin.z);
		glVertex3f(ray.mOrigin.x + 50 * ray.mDir.x, ray.mOrigin.y + 50 * ray.mDir.y, ray.mOrigin.z + 50 * ray.mDir.z);
		glEnd();


		if(cacheSelection) //TODO: cache this differently, and draw this differently
		{ 
			if(selectionCache == 0)
				selectionCache = glGenLists(1);
			glNewList(selectionCache, GL_COMPILE);
			glBegin(GL_LINES);
			for (auto n : selectedNodes)
			{
				vrlib::tien::components::ModelRenderer* r = n->getComponent<vrlib::tien::components::ModelRenderer>();
				if (r)
				{
					auto triangles = r->model->getIndexedTriangles(); //TODO: cache this !
					for (size_t i = 0; i < triangles.first.size(); i += 3)
					{
						for (int ii = 0; ii < 3; ii++)
						{
							glVertex3fv(glm::value_ptr(triangles.second[triangles.first[i + ii]]));
							glVertex3fv(glm::value_ptr(triangles.second[triangles.first[i + (ii + 1) % 3]]));
						}
					}
				}
			}
			glEnd();
			glEndList();
			cacheSelection = false;
		}
		if(selectionCache > 0)
			glCallList(selectionCache);






	}




}

void TienEdit::mouseMove(int x, int y)
{
	mouseState.pos.x = x;
	mouseState.pos.y = y;
	menuOverlay.mousePos = glm::vec2(x, y);
}

void TienEdit::mouseScroll(int offset)
{
}


void TienEdit::mouseDown(MouseButton button)
{
	mouseState.buttons[(int)button] = true;
}

void TienEdit::keyDown(int button)
{
	NormalApp::keyDown(button);
}

void TienEdit::keyUp(int button)
{
	NormalApp::keyUp(button);
	if (focussedComponent && !mouseState.middle)
		focussedComponent->keyUp(button);
}

void TienEdit::keyChar(char character)
{
	NormalApp::keyChar(character);
	if (focussedComponent && !mouseState.middle)
		focussedComponent->keyChar(character);
}


void TienEdit::mouseUp(MouseButton button)
{
	mouseState.buttons[(int)button] = false;
	//TODO if click/mouse didn't move too much
	if(button != vrlib::MouseButtonDeviceDriver::MouseButton::Middle)
	{
		if (menuOverlay.click(button == vrlib::MouseButtonDeviceDriver::MouseButton::Left))
			return;

		Component* clickedComponent = mainPanel->getComponentAt(mouseState.pos);

		if (focussedComponent != clickedComponent)
		{
			if (focussedComponent)
			{
				focussedComponent->focussed = false;
				focussedComponent->unfocus();
			}
			focussedComponent = clickedComponent;
			if (focussedComponent)
			{
				focussedComponent->focussed = true;
				focussedComponent->focus();
			}
		}


		if (mainPanel->click(button == vrlib::MouseButtonDeviceDriver::MouseButton::Left, mouseState.pos))
			return;
	}

	if (button == vrlib::MouseButtonDeviceDriver::MouseButton::Left)
	{
		if (renderPanel->inComponent(mouseState.pos))
		{
			glm::ivec2 mousePos = mouseState.pos;
			mousePos.y = kernel->getWindowHeight() - mousePos.y;
			glm::mat4 cameraMat = glm::translate(glm::toMat4(cameraRot), -cameraPos);
			glm::mat4 projection = glm::perspective(70.0f, renderPanel->size.x / (float)renderPanel->size.y, 0.01f, 500.0f);
			glm::vec4 Viewport(renderPanel->position.x, kernel->getWindowHeight() - renderPanel->position.y - renderPanel->size.y, renderPanel->size.x, renderPanel->size.y);
			glm::vec3 retNear = glm::unProject(glm::vec3(mousePos, 0), cameraMat, projection, glm::vec4(Viewport[0], Viewport[1], Viewport[2], Viewport[3]));
			glm::vec3 retFar = glm::unProject(glm::vec3(mousePos, 1), cameraMat, projection, glm::vec4(Viewport[0], Viewport[1], Viewport[2], Viewport[3]));
			ray = vrlib::math::Ray(retNear, glm::normalize(retFar - retNear));


			vrlib::tien::Node* closestClickedNode = nullptr;
			float closest = 99999999.0f;
			tien.scene.castRay(ray, [&, this](vrlib::tien::Node* node, float hitFraction, const glm::vec3 &hitPosition, const glm::vec3 &hitNormal)
			{
				if (hitFraction < closest)
				{
					hitFraction = closest;
					closestClickedNode = node;
				}
				return true;
			}, false);

			if (closestClickedNode != nullptr)
			{
				vrlib::logger << "Clicked on " << closestClickedNode->name << vrlib::Log::newline;
				perform(new SelectionChangeAction(this, { closestClickedNode }));
			}
			else
				perform(new SelectionChangeAction(this, {}));
			


		}

	}


}


void TienEdit::showBrowsePanel()
{
	if (!modelBrowsePanel)
	{
		class BrowsePanel : public Panel
		{
		public:
			virtual void onReposition(Component* parent) override
			{
				int count = size.x / 128;
				int index = 0;
				for (size_t i = 0; i < components.size(); i++)
				{
					int x = (index % count) * 128;
					int y = (index / count) * 150;
					if (dynamic_cast<Image*>(components[i]))
					{
						components[i]->position.x = x;
						components[i]->position.y = y;
					}
					if (dynamic_cast<Label*>(components[i]))
					{
						components[i]->position.x = x;
						components[i]->position.y = y+130;
						index++;
					}

				}
				Panel::onReposition(parent);
			}
		};
		modelBrowsePanel = new BrowsePanel();
		buildBrowsePanel("./data/models/");
	}
	mainPanel->components[1] = modelBrowsePanel;
	mainPanel->onReposition(nullptr);
}

void TienEdit::buildBrowsePanel(const std::string & directory)
{
	focussedComponent = nullptr;
	for (auto c : modelBrowsePanel->components)
		delete c;
	modelBrowsePanel->components.clear();

	std::vector<std::string> files = vrlib::util::scandir(directory);
	files.erase(std::remove_if(files.begin(), files.end(), [](const std::string &s)
	{
		if (s.size() == 0)
			return true;
		if (s[0] == '.')
			return true;
		if (s[s.length() - 1] == '/')
			return false;
		if (s.find("."))
		{
			std::string extension = s.substr(s.rfind("."));
			if (extension == ".fbx" || extension == ".obj" || extension == ".ma" || extension == ".lwo" || extension == ".stl" || extension == ".dae")
				return false;
			else 
				return true;
		}
		return false;
	}), files.end());

	if (directory != "./")
		files.insert(files.begin(), "../");

	for (size_t i = 0; i < files.size(); i++)
	{
		Image* img = nullptr;
		if (files[i][files[i].size() - 1] == '/')
		{
			img = new Image(menuOverlay.skinTexture, glm::ivec2(0, 0), glm::ivec2(128, 128), glm::ivec2(333, 0), glm::ivec2(333 + 128, 128));
			img->onClick = [this, directory, i, files]()
			{
				std::string newDirectory = directory + files[i];
				if (files[i] == "../")
					newDirectory = directory.substr(0, directory.rfind("/", directory.size()-2)) + "/";
				buildBrowsePanel(newDirectory);
			};
		}
		else
		{
			img = new Image(menuOverlay.skinTexture, glm::ivec2(0, 0), glm::ivec2(128, 128), glm::ivec2(333, 0), glm::ivec2(333 + 128, 128));
			img->onClick = [this, directory, i, files]()
			{
				browseCallback(directory + files[i]);
				mainPanel->components[1] = renderPanel;
			};

		}
		if (img)
			modelBrowsePanel->components.push_back(img);
		modelBrowsePanel->components.push_back(new Label(files[i], glm::ivec2(0,0)));
	}
	mainPanel->onReposition(nullptr);
}


void TienEdit::perform(Action* action)
{
	action->perform(this);
	actions.push_back(action);
}