#include "NGLScene.h"
#include <QGuiApplication>
#include <QMouseEvent>

#include <ngl/NGLInit.h>
#include <ngl/NGLStream.h>
#include <ngl/Random.h>
#include <ngl/ShaderLib.h>

constexpr auto ComputeShader="ComputeShader";
constexpr auto ParticleShader="ParticleShader";

NGLScene::NGLScene()
{
  setTitle( "Qt5 Simple NGL Demo" );
}


NGLScene::~NGLScene()
{
  std::cout << "Shutting down NGL, removing VAO's and Shaders\n";
}



void NGLScene::resizeGL( int _w, int _h )
{
  m_win.width  = static_cast<int>( _w * devicePixelRatio() );
  m_win.height = static_cast<int>( _h * devicePixelRatio() );
}


void NGLScene::initializeGL()
{
  // we must call that first before any other GL commands to load and link the
  // gl commands from the lib, if that is not done program will crash
  ngl::NGLInit::instance();
  glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ); //  Background
  // enable depth testing for drawing
  glEnable( GL_DEPTH_TEST );
// enable multisampling for smoother drawing
#ifndef USINGIOS_
  glEnable( GL_MULTISAMPLE );
#endif
  // now to load the shader and set the values
  // grab an instance of shader manager
  ngl::ShaderLib* shader = ngl::ShaderLib::instance();
  // create the shader program
  shader->createShaderProgram( ParticleShader );
  // now we are going to create empty shaders for Frag and Vert
  constexpr auto vertexShader  = "ComputeVertex";
  constexpr auto fragShader    = "ComputeFragment";

  shader->attachShader( vertexShader, ngl::ShaderType::VERTEX );
  shader->attachShader( fragShader, ngl::ShaderType::FRAGMENT );
  // attach the source
  shader->loadShaderSource( vertexShader, "shaders/ParticlesVertex.glsl" );
  shader->loadShaderSource( fragShader, "shaders/ParticlesFragment.glsl" );

  // compile the shaders
  shader->compileShader( vertexShader );
  shader->compileShader( fragShader );
  // add them to the program
  shader->attachShaderToProgram( ParticleShader, vertexShader );
  shader->attachShaderToProgram( ParticleShader, fragShader );
  shader->linkProgramObject(ParticleShader);
  shader->use(ParticleShader);


  constexpr auto compute = "Compute";

  shader->createShaderProgram( ComputeShader );
  shader->attachShader( compute, ngl::ShaderType::COMPUTE );
  shader->loadShaderSource( compute, "shaders/ParticlesCompute.glsl" );
  shader->compileShader( compute );
  shader->attachShaderToProgram( ComputeShader, compute );
  shader->linkProgramObject(ComputeShader);
  shader->use(ComputeShader);
  createComputeBuffers();
  startTimer(10);
  m_attractorUpdateTimer=startTimer(100);

}

void NGLScene::createComputeBuffers()
{
  glGenVertexArrays(1,&m_vao);
  glBindVertexArray(m_vao);
  std::vector<ngl::Vec4> pos(c_numParticles);
  ngl::Random *rng=ngl::Random::instance();
  for(auto &p : pos)
  {
    p.m_x=rng->randomNumber(10);
    p.m_y=rng->randomNumber(10);
    p.m_z=rng->randomNumber(10);
    p.m_w=rng->randomPositiveNumber(1.0f);
  }

  glGenBuffers(1, &m_positionBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, m_positionBufferID);
  glBufferData(GL_ARRAY_BUFFER, c_numParticles * sizeof(ngl::Vec4), &pos[0].m_x, GL_DYNAMIC_COPY);


  std::vector<ngl::Vec3> vel(c_numParticles);
  for(auto &v : vel)
  {
    v.m_x=rng->randomNumber(0.5f);
    v.m_y=rng->randomNumber(0.5f);
    v.m_z=rng->randomNumber(0.5f);
  }
  glGenBuffers(1, &m_velocityBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, m_velocityBufferID);
  glBufferData(GL_ARRAY_BUFFER, vel.size() * sizeof(ngl::Vec3), &vel[0].m_x, GL_DYNAMIC_COPY);

  std::vector<ngl::Vec3> attractors(c_numAttractors);
  for(auto &a : attractors)
  {
    a=rng->getRandomPoint(20,20,20);
  }
  glGenBuffers(1, &m_attractorBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, m_attractorBufferID);
  glBufferData(GL_ARRAY_BUFFER, attractors.size() * sizeof(ngl::Vec3), &attractors[0].m_x, GL_DYNAMIC_COPY);

  glBindVertexArray(0);
}



void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib* shader = ngl::ShaderLib::instance();

}

void NGLScene::paintGL()
{
  glViewport( 0, 0, m_win.width, m_win.height );
  // clear the screen and depth buffer
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

  // grab an instance of the shader manager
  ngl::ShaderLib* shader = ngl::ShaderLib::instance();

  // Rotation based on the mouse position for our global transform
  ngl::Mat4 rotX;
  ngl::Mat4 rotY;
  // create the rotation matrices
  rotX.rotateX( m_win.spinXFace );
  rotY.rotateY( m_win.spinYFace );
  // multiply the rotations
  m_mouseGlobalTX = rotX * rotY;
  // add the translations
  m_mouseGlobalTX.m_m[ 3 ][ 0 ] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[ 3 ][ 1 ] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[ 3 ][ 2 ] = m_modelPos.m_z;

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);

  glDisable(GL_CULL_FACE);

  glBindVertexArray(m_vao);
  shader->use(ComputeShader);
  shader->setUniform("dt",m_dt);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_positionBufferID);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_velocityBufferID);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_attractorBufferID);

  glDispatchCompute(c_numParticles / 128, 1, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);


  shader->use(ParticleShader);
  ngl::Mat4 MVP= ngl::perspective(45.0f,float(width())/height(),0.5f,100.0f) *
                 ngl::lookAt(ngl::Vec3(25,25,25),ngl::Vec3::zero(),ngl::Vec3::up()) *
                 m_mouseGlobalTX;
  shader->setUniform("MVP",MVP);

  glBindBuffer (GL_ARRAY_BUFFER, m_positionBufferID);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,0, 0);
  glDrawArrays(GL_POINTS, 0, c_numParticles);

}


void NGLScene::timerEvent(QTimerEvent *_event)
{
  if(_event->timerId()== m_attractorUpdateTimer)
  {

    ngl::Random *rng=ngl::Random::instance();

    std::vector<ngl::Vec3> attractors(c_numAttractors);
    for(auto &a : attractors)
    {
      //a=rng->getRandomPoint(10,10,10);
      a.m_x=sinf(0.5f)*rng->randomPositiveNumber(5);
      a.m_y=cosf(0.5f)*rng->randomPositiveNumber(5);
      a.m_z=tanf(0.5f);

    }
    glGenBuffers(1, &m_attractorBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, m_attractorBufferID);
    glBufferData(GL_ARRAY_BUFFER, attractors.size() * sizeof(ngl::Vec3), &attractors[0].m_x, GL_DYNAMIC_COPY);
  }
  update();
}



//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent( QKeyEvent* _event )
{
  // that method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch ( _event->key() )
  {
    // escape key to quit
    case Qt::Key_Escape:
      QGuiApplication::exit( EXIT_SUCCESS );
      break;
// turn on wirframe rendering
#ifndef USINGIOS_
    case Qt::Key_W:
      glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
      break;
    // turn off wire frame
    case Qt::Key_S:
      glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
      break;
#endif
    // show full screen
    case Qt::Key_F:
      showFullScreen();
      break;
    // show windowed
    case Qt::Key_N:
      showNormal();
      break;
    case Qt::Key_Space :
      m_win.spinXFace=0;
      m_win.spinYFace=0;
      m_modelPos.set(ngl::Vec3::zero());
      m_dt=0.5f;
    break;

    case Qt::Key_Up :
      m_dt+=0.01f;
    break;
  case Qt::Key_Down :
    m_dt-=0.01f;
  break;


    default:
      break;
  }
  update();
}
