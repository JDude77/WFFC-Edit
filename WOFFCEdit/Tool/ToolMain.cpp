#include "ToolMain.h"
#include "../Resources/resource.h"
#include <vector>
#include <sstream>

//ToolMain Class
ToolMain::ToolMain()
{
	m_currentChunk = 0;				//Default chunk value
	m_selectedObject = 0;			//Initial selection ID
	m_sceneGraph.clear();			//Clear the vector for the scenegraph
	m_databaseConnection = nullptr;

	m_executeOnce = false;

	//Zero input commands
	m_toolInputCommands.forward					= false;
	m_toolInputCommands.back					= false;
	m_toolInputCommands.left					= false;
	m_toolInputCommands.right					= false;
	m_toolInputCommands.rotLeft					= false;
	m_toolInputCommands.rotRight				= false;
	m_toolInputCommands.activateCameraMovement	= false;
	m_toolInputCommands.moveSelectedObject		= false;
	m_toolInputCommands.mousePickingActive		= false;
	m_toolInputCommands.copy					= false;
	m_toolInputCommands.cut						= false;
	m_toolInputCommands.paste					= false;
	m_toolInputCommands.deleteObject			= false;
	m_toolInputCommands.increaseMoveSpeed		= false;
	m_toolInputCommands.save					= false;
	m_toolInputCommands.wireframeMode			= false;
	m_toolInputCommands.mouseX					= 0.0f;
	m_toolInputCommands.mouseY					= 0.0f;
}//End default constructor

ToolMain::~ToolMain()
{
	//Close the database connection
	sqlite3_close(m_databaseConnection);
}//End destructor

int ToolMain::getCurrentSelectionID()
{
	return m_selectedObject;
}//End getCurrentSelectionID

void ToolMain::onActionInitialise(HWND handle, int width, int height)
{
	//Window size, handle etc. for DirectX
	m_width		= width;
	m_height	= height;
	m_d3dRenderer.Initialize(handle, m_width, m_height);

	//Establish database connection
	const int rc = sqlite3_open_v2("database/test.db", &m_databaseConnection, SQLITE_OPEN_READWRITE, NULL);

	if (rc) 
	{
		TRACE("Can't open database\n");
	}//End if
	else 
	{
		TRACE("Opened database successfully\n");
	}//End else

	onActionLoad();
}//End onActionInitialise

void ToolMain::onActionLoad()
{
	//Load current chunk and objects into lists
	if (!m_sceneGraph.empty())
	{
		m_sceneGraph.clear();
	}//End if
	
	sqlite3_stmt *pResults;
	sqlite3_stmt *pResultsChunk;

	//SQL command which will return all records from the objects table.
	const char* sqlCommand = "SELECT * from Objects";				
	//Send command and fill result object
	int rc = sqlite3_prepare_v2(m_databaseConnection, sqlCommand, -1, &pResults, 0);
	
	//Loop for each row in results until there are no more rows
	//For every row in the results, we create an object
	while (sqlite3_step(pResults) == SQLITE_ROW)
	{	
		SceneObject newSceneObject;
		newSceneObject.ID =						sqlite3_column_int(pResults,								0);
		newSceneObject.chunk_ID =				sqlite3_column_int(pResults,								1);
		newSceneObject.model_path =				reinterpret_cast<const char*>(sqlite3_column_text(pResults, 2));
		newSceneObject.tex_diffuse_path =		reinterpret_cast<const char*>(sqlite3_column_text(pResults, 3));
		newSceneObject.posX =					sqlite3_column_double(pResults,								4);
		newSceneObject.posY =					sqlite3_column_double(pResults,								5);
		newSceneObject.posZ =					sqlite3_column_double(pResults,								6);
		newSceneObject.rotX =					sqlite3_column_double(pResults,								7);
		newSceneObject.rotY =					sqlite3_column_double(pResults,								8);
		newSceneObject.rotZ =					sqlite3_column_double(pResults,								9);
		newSceneObject.scaX =					sqlite3_column_double(pResults,								10);
		newSceneObject.scaY =					sqlite3_column_double(pResults,								11);
		newSceneObject.scaZ =					sqlite3_column_double(pResults,								12);
		newSceneObject.render =					sqlite3_column_int(pResults,								13);
		newSceneObject.collision =				sqlite3_column_int(pResults,								14);
		newSceneObject.collision_mesh =			reinterpret_cast<const char*>(sqlite3_column_text(pResults, 15));
		newSceneObject.collectable =			sqlite3_column_int(pResults,								16);
		newSceneObject.destructable =			sqlite3_column_int(pResults,								17);
		newSceneObject.health_amount =			sqlite3_column_int(pResults,								18);
		newSceneObject.editor_render =			sqlite3_column_int(pResults,								19);
		newSceneObject.editor_texture_vis =		sqlite3_column_int(pResults,								20);
		newSceneObject.editor_normals_vis =		sqlite3_column_int(pResults,								21);
		newSceneObject.editor_collision_vis =	sqlite3_column_int(pResults,								22);
		newSceneObject.editor_pivot_vis =		sqlite3_column_int(pResults,								23);
		newSceneObject.pivotX =					sqlite3_column_double(pResults,								24);
		newSceneObject.pivotY =					sqlite3_column_double(pResults,								25);
		newSceneObject.pivotZ =					sqlite3_column_double(pResults,								26);
		newSceneObject.snapToGround =			sqlite3_column_int(pResults,								27);
		newSceneObject.AINode =					sqlite3_column_int(pResults,								28);
		newSceneObject.audio_path =				reinterpret_cast<const char*>(sqlite3_column_text(pResults, 29));
		newSceneObject.volume =					sqlite3_column_double(pResults,								30);
		newSceneObject.pitch =					sqlite3_column_double(pResults,								31);
		newSceneObject.pan =					sqlite3_column_int(pResults,								32);
		newSceneObject.one_shot =				sqlite3_column_int(pResults,								33);
		newSceneObject.play_on_init =			sqlite3_column_int(pResults,								34);
		newSceneObject.play_in_editor =			sqlite3_column_int(pResults,								35);
		newSceneObject.min_dist =				sqlite3_column_double(pResults,								36);
		newSceneObject.max_dist =				sqlite3_column_double(pResults,								37);
		newSceneObject.camera =					sqlite3_column_int(pResults,								38);
		newSceneObject.path_node =				sqlite3_column_int(pResults,								39);
		newSceneObject.path_node_start =		sqlite3_column_int(pResults,								40);
		newSceneObject.path_node_end =			sqlite3_column_int(pResults,								41);
		newSceneObject.parent_id =				sqlite3_column_int(pResults,								42);
		newSceneObject.editor_wireframe =		sqlite3_column_int(pResults,								43);
		newSceneObject.name =					reinterpret_cast<const char*>(sqlite3_column_text(pResults, 44));
		newSceneObject.light_type =				sqlite3_column_int(pResults,								45);
		newSceneObject.light_diffuse_r =		sqlite3_column_double(pResults,								46);
		newSceneObject.light_diffuse_g =		sqlite3_column_double(pResults,								47);
		newSceneObject.light_diffuse_b =		sqlite3_column_double(pResults,								48);
		newSceneObject.light_specular_r =		sqlite3_column_double(pResults,								49);
		newSceneObject.light_specular_g =		sqlite3_column_double(pResults,								50);
		newSceneObject.light_specular_b =		sqlite3_column_double(pResults,								51);
		newSceneObject.light_spot_cutoff =		sqlite3_column_double(pResults,								52);
		newSceneObject.light_constant =			sqlite3_column_double(pResults,								53);
		newSceneObject.light_linear =			sqlite3_column_double(pResults,								54);
		newSceneObject.light_quadratic =		sqlite3_column_double(pResults,								55);

		//Send completed object to scenegraph
		m_sceneGraph.push_back(newSceneObject);
	}//End while

	//THE WORLD CHUNK
	//SQL command which will return all records from the chunks table.
	sqlCommand = "SELECT * from Chunks";
	rc = sqlite3_prepare_v2(m_databaseConnection, sqlCommand, -1, &pResultsChunk, nullptr);
	
	sqlite3_step(pResultsChunk);
	m_chunk.ID =						sqlite3_column_int(pResultsChunk,									0);
	m_chunk.name =						reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	1));
	m_chunk.chunk_x_size_metres =		sqlite3_column_int(pResultsChunk,									2);
	m_chunk.chunk_y_size_metres =		sqlite3_column_int(pResultsChunk,									3);
	m_chunk.chunk_base_resolution =		sqlite3_column_int(pResultsChunk,									4);
	m_chunk.heightmap_path =			reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	5));
	m_chunk.tex_diffuse_path =			reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	6));
	m_chunk.tex_splat_alpha_path =		reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	7));
	m_chunk.tex_splat_1_path =			reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	8));
	m_chunk.tex_splat_2_path =			reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	9));
	m_chunk.tex_splat_3_path =			reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	10));
	m_chunk.tex_splat_4_path =			reinterpret_cast<const char*>(sqlite3_column_text(pResultsChunk,	11));
	m_chunk.render_wireframe =			sqlite3_column_int(pResultsChunk,									12);
	m_chunk.render_normals =			sqlite3_column_int(pResultsChunk,									13);
	m_chunk.tex_diffuse_tiling =		sqlite3_column_int(pResultsChunk,									14);
	m_chunk.tex_splat_1_tiling =		sqlite3_column_int(pResultsChunk,									15);
	m_chunk.tex_splat_2_tiling =		sqlite3_column_int(pResultsChunk,									16);
	m_chunk.tex_splat_3_tiling =		sqlite3_column_int(pResultsChunk,									17);
	m_chunk.tex_splat_4_tiling =		sqlite3_column_int(pResultsChunk,									18);

	//Process results into renderable
	m_d3dRenderer.BuildDisplayList(&m_sceneGraph);
	//Build the renderable chunk 
	m_d3dRenderer.BuildDisplayChunk(&m_chunk);
}//End onActionLoad

void ToolMain::onActionSave()
{
	//Update the scene graph with the current data
	m_sceneGraph.clear();
	const std::vector<DisplayObject> currentDisplayList = m_d3dRenderer.GetDisplayList();

	//Go through every member of the display list
	for(int i = 0; i < currentDisplayList.size(); i++)
	{
		SceneObject newSceneObject;
        newSceneObject.ID = currentDisplayList.at(i).m_ID;
        newSceneObject.chunk_ID = currentDisplayList.at(i).m_ID;
        newSceneObject.model_path = WCHARTToString(currentDisplayList.at(i).m_model_path);
        newSceneObject.tex_diffuse_path =  WCHARTToString(currentDisplayList.at(i).m_texture_diffuse_path);
        newSceneObject.posX = currentDisplayList.at(i).m_position.x;
        newSceneObject.posY = currentDisplayList.at(i).m_position.y;
        newSceneObject.posZ = currentDisplayList.at(i).m_position.z;
        newSceneObject.rotX = currentDisplayList.at(i).m_orientation.x;
        newSceneObject.rotY = currentDisplayList.at(i).m_orientation.y;
        newSceneObject.rotZ = currentDisplayList.at(i).m_orientation.z;
        newSceneObject.scaX = currentDisplayList.at(i).m_scale.x;
        newSceneObject.scaY = currentDisplayList.at(i).m_scale.y;
        newSceneObject.scaZ = currentDisplayList.at(i).m_scale.z;

        m_sceneGraph.push_back(newSceneObject);
	}//End for

	sqlite3_stmt *pResults;

	//Delete all in-world objects
	const char* sqlCommand = "DELETE FROM Objects";	
	int rc = sqlite3_prepare_v2(m_databaseConnection, sqlCommand, -1, &pResults, 0);
	sqlite3_step(pResults);

	//Populate with new objects
	const int numObjects = m_sceneGraph.size();
	std::string sqlCommand2;

	//Loop through the scene graph
	for (int i = 0; i < numObjects; i++)
	{
		std::stringstream command;
		command << "INSERT INTO Objects " 
			<<"VALUES(" << m_sceneGraph.at(i).ID << ","
			<< m_sceneGraph.at(i).chunk_ID  << ","
			<< "'" << m_sceneGraph.at(i).model_path <<"'" << ","
			<< "'" << m_sceneGraph.at(i).tex_diffuse_path << "'" << ","
			<< m_sceneGraph.at(i).posX << ","
			<< m_sceneGraph.at(i).posY << ","
			<< m_sceneGraph.at(i).posZ << ","
			<< m_sceneGraph.at(i).rotX << ","
			<< m_sceneGraph.at(i).rotY << ","
			<< m_sceneGraph.at(i).rotZ << ","
			<< m_sceneGraph.at(i).scaX << ","
			<< m_sceneGraph.at(i).scaY << ","
			<< m_sceneGraph.at(i).scaZ << ","
			<< m_sceneGraph.at(i).render << ","
			<< m_sceneGraph.at(i).collision << ","
			<< "'" << m_sceneGraph.at(i).collision_mesh << "'" << ","
			<< m_sceneGraph.at(i).collectable << ","
			<< m_sceneGraph.at(i).destructable << ","
			<< m_sceneGraph.at(i).health_amount << ","
			<< m_sceneGraph.at(i).editor_render << ","
			<< m_sceneGraph.at(i).editor_texture_vis << ","
			<< m_sceneGraph.at(i).editor_normals_vis << ","
			<< m_sceneGraph.at(i).editor_collision_vis << ","
			<< m_sceneGraph.at(i).editor_pivot_vis << ","
			<< m_sceneGraph.at(i).pivotX << ","
			<< m_sceneGraph.at(i).pivotY << ","
			<< m_sceneGraph.at(i).pivotZ << ","
			<< m_sceneGraph.at(i).snapToGround << ","
			<< m_sceneGraph.at(i).AINode << ","
			<< "'" << m_sceneGraph.at(i).audio_path << "'" << ","
			<< m_sceneGraph.at(i).volume << ","
			<< m_sceneGraph.at(i).pitch << ","
			<< m_sceneGraph.at(i).pan << ","
			<< m_sceneGraph.at(i).one_shot << ","
			<< m_sceneGraph.at(i).play_on_init << ","
			<< m_sceneGraph.at(i).play_in_editor << ","
			<< m_sceneGraph.at(i).min_dist << ","
			<< m_sceneGraph.at(i).max_dist << ","
			<< m_sceneGraph.at(i).camera << ","
			<< m_sceneGraph.at(i).path_node << ","
			<< m_sceneGraph.at(i).path_node_start << ","
			<< m_sceneGraph.at(i).path_node_end << ","
			<< m_sceneGraph.at(i).parent_id << ","
			<< m_sceneGraph.at(i).editor_wireframe << ","
			<< "'" << m_sceneGraph.at(i).name << "'" << ","
			<< m_sceneGraph.at(i).light_type << ","
			<< m_sceneGraph.at(i).light_diffuse_r << ","
			<< m_sceneGraph.at(i).light_diffuse_g << ","
			<< m_sceneGraph.at(i).light_diffuse_b << ","
			<< m_sceneGraph.at(i).light_specular_r << ","
			<< m_sceneGraph.at(i).light_specular_g << ","
			<< m_sceneGraph.at(i).light_specular_b << ","
			<< m_sceneGraph.at(i).light_spot_cutoff << ","
			<< m_sceneGraph.at(i).light_constant << ","
			<< m_sceneGraph.at(i).light_linear << ","
			<< m_sceneGraph.at(i).light_quadratic
			<< ")";
		sqlCommand2 = command.str();
		sqlite3_prepare_v2(m_databaseConnection, sqlCommand2.c_str(), -1, &pResults, nullptr);
		sqlite3_step(pResults);	
	}
	MessageBox(nullptr, L"Objects Saved", L"Notification", MB_OK);
}//End onActionSave

void ToolMain::onActionSaveTerrain()
{
	m_d3dRenderer.SaveDisplayChunk(&m_chunk);
}//End onActionSaveTerrain

void ToolMain::onActionUndo()
{
	m_d3dRenderer.Undo(m_selectedObject, m_selectedObject);
}//End onActionUndo

void ToolMain::onActionRedo()
{
	m_d3dRenderer.Redo(m_selectedObject, m_selectedObject);
}//End onActionRedo

void ToolMain::onActionCopy()
{
	m_d3dRenderer.Copy(m_selectedObject);
}//End onActionCopy

void ToolMain::onActionCut()
{
	m_d3dRenderer.Cut(m_selectedObject);
}//End onActionCut

void ToolMain::onActionPaste()
{
	m_d3dRenderer.Paste();
}//End onActionPaste

void ToolMain::onActionDelete()
{
	m_d3dRenderer.Delete(m_selectedObject);
}//End onActionDelete

void ToolMain::onActionWireframe()
{
	m_d3dRenderer.ToggleWireframe();
}//End onActionWireframe

void ToolMain::Tick(MSG *msg, const bool selectWindowOpen, const int selectWindowPreviousSelected)
{
	//Do we have a selection
	//Do we have a mode
	//Are we clicking/dragging/releasing
	if(!m_executeOnce)
	{
		if(!selectWindowOpen && m_toolInputCommands.mousePickingActive)
		{
			m_executeOnce = true;
			m_d3dRenderer.MoveSelectedObjectStart(m_selectedObject);
			m_toolInputCommands.mousePickingActive = false;
		}//End if

		if(m_toolInputCommands.deleteObject)
		{
			m_executeOnce = true;
			m_d3dRenderer.Delete(m_selectedObject);
		}//End if

		if(m_toolInputCommands.cut)
		{
			m_executeOnce = true;
			m_d3dRenderer.Cut(m_selectedObject);
		}//End if

		if(m_toolInputCommands.copy)
		{
			m_executeOnce = true;
			m_d3dRenderer.Copy(m_selectedObject);
		}//End if

		if(m_toolInputCommands.paste)
		{
			m_executeOnce = true;
			m_d3dRenderer.Paste();
		}//End if

		if(m_toolInputCommands.undo)
		{
			m_executeOnce = true;
			m_d3dRenderer.Undo(m_selectedObject, m_selectedObject);
		}//End if

		if(m_toolInputCommands.redo)
		{
			m_executeOnce = true;
			m_d3dRenderer.Redo(m_selectedObject, m_selectedObject);
		}//End if

		if(m_toolInputCommands.save)
		{
			m_executeOnce = true;
			m_toolInputCommands.save = false;
			onActionSave();
			//Due to the separate window opening, need to manually disable both keys
			m_keyArray[VK_CONTROL] = false;
			m_keyArray['S'] = false;
		}//End if

		if(m_toolInputCommands.wireframeMode)
		{
			m_executeOnce = true;
			m_d3dRenderer.ToggleWireframe();
		}//End if
	}//End if

	//There is definitely a better way to do this, but this works, so it's staying for now
	else if
	(!(	m_toolInputCommands.redo				||
		m_toolInputCommands.undo				||
		m_toolInputCommands.paste				||
		m_toolInputCommands.copy				||
		m_toolInputCommands.cut					|| 
		m_toolInputCommands.deleteObject		||
		m_toolInputCommands.mousePickingActive	||
		m_toolInputCommands.save				||
		m_toolInputCommands.wireframeMode))
	{
		m_executeOnce = false;
	}//End else if

	//Hande moving a selected object
	if(!selectWindowOpen && m_toolInputCommands.moveSelectedObject)
	{
		m_d3dRenderer.MoveSelectedObject(m_selectedObject);
	}//End if

	//Update the highlighted object when the select window is open
	if(selectWindowOpen)
	{
		m_d3dRenderer.HighlightSelectedObject(selectWindowPreviousSelected, m_selectedObject);
	}//End if

	//Has something changed
		//Update scenegraph
		//Add to scenegraph
		//Resend scenegraph to Direct X renderer

	//Renderer Update Call
	m_d3dRenderer.Tick(&m_toolInputCommands);
}//End Tick

void ToolMain::UpdateInput(const MSG* msg)
{
	switch (msg->message)
	{
		//Global inputs
		case WM_KEYDOWN:
			m_keyArray[msg->wParam] = true;
			break;
		case WM_KEYUP:
			m_keyArray[msg->wParam] = false;
			break;

		//Mouse movement
		case WM_MOUSEMOVE:
			m_toolInputCommands.mouseX = GET_X_LPARAM(msg->lParam);
			m_toolInputCommands.mouseY = GET_Y_LPARAM(msg->lParam);
			break;

		//Mouse left button down
		case WM_LBUTTONDOWN:
			m_toolInputCommands.mousePickingActive = true;
			m_toolInputCommands.moveSelectedObject = true;
			break;

		//Mouse left button up
		case WM_LBUTTONUP:
			m_toolInputCommands.mousePickingActive = false;
			m_toolInputCommands.moveSelectedObject = false;
			if(m_d3dRenderer.GetCurrentDragActive())
				m_d3dRenderer.MoveSelectedObjectEnd(m_selectedObject, m_selectedObject);
			break;

		//Mouse right button down
		case WM_RBUTTONDOWN:	
			m_toolInputCommands.activateCameraMovement = true;
			//Set flag for the mouse button in inputcommands
			break;

		//Mouse right button up
		case WM_RBUTTONUP:
			m_toolInputCommands.activateCameraMovement = false;
			break;
	}//End switch

	//Update all the actual app functionality that we want
	//Information will either be used in toolmain, or sent down to the renderer (Camera movement, etc.)

	//Undo/redo commands
	m_toolInputCommands.undo = m_keyArray[VK_CONTROL] && m_keyArray['Z'] ? true : false;
	m_toolInputCommands.redo = m_keyArray[VK_CONTROL] && m_keyArray['Y'] ? true : false;

	//Delete command
	m_toolInputCommands.deleteObject =	m_keyArray[VK_DELETE] ? true : false;

	//Copy/cut/paste commands
	m_toolInputCommands.copy =		m_keyArray[VK_CONTROL] && m_keyArray['C'] ? true : false;
	m_toolInputCommands.cut =		m_keyArray[VK_CONTROL] && m_keyArray['X'] ? true : false;
	m_toolInputCommands.paste =		m_keyArray[VK_CONTROL] && m_keyArray['V'] ? true : false;

	//MFC interface keyboard commands
	m_toolInputCommands.save =			m_keyArray[VK_CONTROL] && m_keyArray['S'] ? true : false;
	m_toolInputCommands.wireframeMode = m_keyArray['1'] || m_keyArray[VK_NUMPAD1] ? true : false;

	//WASD movement (disabled when holding control)
	if(!m_keyArray[VK_CONTROL])
	{
		m_toolInputCommands.forward =	m_keyArray['W'] ? true : false;
		m_toolInputCommands.back =	    m_keyArray['S'] ? true : false;
		m_toolInputCommands.left =		m_keyArray['A'] ? true : false;
		m_toolInputCommands.right =		m_keyArray['D'] ? true : false;
	}//End if

	//Rotation
	m_toolInputCommands.rotRight =	m_keyArray['E'] ? true : false;
	m_toolInputCommands.rotLeft =	m_keyArray['Q'] ? true : false;

	//Camera speed
	m_toolInputCommands.increaseMoveSpeed = m_keyArray[VK_SHIFT] ? true : false;
}//End UpdateInput