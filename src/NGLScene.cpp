#include "NGLScene.h"
#include <QGuiApplication>
#include <QMouseEvent>

#include <ngl/NGLInit.h>
#include <ngl/NGLStream.h>
#include <ngl/Random.h>
#include <ngl/Transformation.h>
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
  m_projection=ngl::perspective(45.0f,float(m_win.width)/m_win.height,0.5f,100.0f);

}


void NGLScene::initializeGL()
{
  // we must call that first before any other GL commands to load and link the
  // gl commands from the lib, if that is not done program will crash
  ngl::NGLInit::initialize();
  glClearColor( 0.0f, 0.0f, 0.0f, 1.0f ); //  Background
  // enable depth testing for drawing
  glEnable( GL_DEPTH_TEST );

  glEnable( GL_MULTISAMPLE );
  
  // create the shader program
  ngl::ShaderLib::createShaderProgram( ParticleShader );
  // now we are going to create empty shaders for Frag and Vert
  constexpr auto vertexShader  = "ComputeVertex";
  constexpr auto fragShader    = "ComputeFragment";

  ngl::ShaderLib::attachShader( vertexShader, ngl::ShaderType::VERTEX );
  ngl::ShaderLib::attachShader( fragShader, ngl::ShaderType::FRAGMENT );
  // attach the source
  ngl::ShaderLib::loadShaderSource( vertexShader, "shaders/ParticlesVertex.glsl" );
  ngl::ShaderLib::loadShaderSource( fragShader, "shaders/ParticlesFragment.glsl" );

  // compile the shaders
  ngl::ShaderLib::compileShader( vertexShader );
  ngl::ShaderLib::compileShader( fragShader );
  // add them to the program
  ngl::ShaderLib::attachShaderToProgram( ParticleShader, vertexShader );
  ngl::ShaderLib::attachShaderToProgram( ParticleShader, fragShader );
  ngl::ShaderLib::linkProgramObject(ParticleShader);
  ngl::ShaderLib::use(ParticleShader);


  constexpr auto compute = "Compute";

  ngl::ShaderLib::createShaderProgram( ComputeShader );
  ngl::ShaderLib::attachShader( compute, ngl::ShaderType::COMPUTE );
  ngl::ShaderLib::loadShaderSource( compute, "shaders/ParticlesCompute.glsl" );
  ngl::ShaderLib::compileShader( compute );
  ngl::ShaderLib::attachShaderToProgram( ComputeShader, compute );
  ngl::ShaderLib::linkProgramObject(ComputeShader);
  ngl::ShaderLib::use(ComputeShader);
  createComputeBuffers();
  startTimer(10);
  m_attractorUpdateTimer=startTimer(800);
  m_elapsedTimer.start();
  ngl::VAOPrimitives::createSphere("sphere",0.2f,10.0f);
  ngl::ShaderLib::use("nglDiffuseShader");

  ngl::ShaderLib::setUniform("Colour",1.0f,1.0f,1.0f,1.0f);
  ngl::ShaderLib::setUniform("lightPos",0.0f,0.0f,1.0f);
  ngl::ShaderLib::setUniform("lightDiffuse",1.0f,1.0f,1.0f,1.0f);
  ngl::Mat3 normalMatrix;
  ngl::ShaderLib::setUniform("normalMatrix",normalMatrix);


  m_view=ngl::lookAt(ngl::Vec3(25,25,25),ngl::Vec3::zero(),ngl::Vec3::up());

  m_projection=ngl::perspective(45.0f,float(width())/height(),0.5f,100.0f);



}

void NGLScene::createComputeBuffers()
{
  glGenVertexArrays(1,&m_vao);
  glBindVertexArray(m_vao);
  std::vector<ngl::Vec4> pos(c_numParticles);
  for(auto &p : pos)
  {
    p.m_x=ngl::Random::randomNumber(40);
    p.m_y=ngl::Random::randomNumber(40);
    p.m_z=ngl::Random::randomNumber(40);
    p.m_w=ngl::Random::randomPositiveNumber(1.0f)+0.1f;
  }

  glGenBuffers(1, &m_positionBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, m_positionBufferID);
  glBufferData(GL_ARRAY_BUFFER, c_numParticles * sizeof(ngl::Vec4), &pos[0].m_x, GL_DYNAMIC_COPY);


  std::vector<ngl::Vec3> vel(c_numParticles);
  for(auto &v : vel)
  {
    v.m_x=0.001f+ngl::Random::randomNumber(0.5f);
    v.m_y=0.001f+ngl::Random::randomNumber(0.5f);
    v.m_z=0.001f+ngl::Random::randomNumber(0.5f);
  }
  glGenBuffers(1, &m_velocityBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, m_velocityBufferID);
  glBufferData(GL_ARRAY_BUFFER, vel.size() * sizeof(ngl::Vec3), &vel[0].m_x, GL_DYNAMIC_COPY);

  m_attractors.resize(c_numAttractors);
  for(auto &a : m_attractors)
  {
    a=ngl::Random::getRandomPoint(20,20,20);
  }
  glGenBuffers(1, &m_attractorBufferID);
  glBindBuffer(GL_ARRAY_BUFFER, m_attractorBufferID);
  glBufferData(GL_ARRAY_BUFFER, m_attractors.size() * sizeof(ngl::Vec3), &m_attractors[0].m_x, GL_DYNAMIC_COPY);

  glBindVertexArray(0);
}



void NGLScene::loadMatricesToShader()
{

}

void NGLScene::paintGL()
{
  glViewport( 0, 0, m_win.width, m_win.height );
  // clear the screen and depth buffer
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );


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
  ngl::ShaderLib::use(ComputeShader);
  m_dt=m_elapsedTimer.elapsed()/60.0f;
  m_elapsedTimer.restart();
  ngl::ShaderLib::setUniform("dt",m_dt);
  std::cout<<m_dt<<'\n';
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_positionBufferID);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_velocityBufferID);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_attractorBufferID);

  glDispatchCompute(c_numParticles / 128, 1, 1);
  glMemoryBarrier(GL_ALL_BARRIER_BITS);


  ngl::ShaderLib::use(ParticleShader);
  ngl::Mat4 MVP= m_projection * m_view * m_mouseGlobalTX;
  ngl::ShaderLib::setUniform("MVP",MVP);

  glBindBuffer (GL_ARRAY_BUFFER, m_positionBufferID);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE,0, 0);
  glDrawArrays(GL_POINTS, 0, c_numParticles);

  glEnable(GL_CULL_FACE);
  ngl::ShaderLib::use("nglDiffuseShader");
  ngl::Transformation tx;
  for(auto p : m_attractors)
  {
    tx.setPosition(p);
    MVP= m_projection * m_view * m_mouseGlobalTX  * tx.getMatrix();
    ngl::ShaderLib::setUniform("MVP",MVP);
    ngl::VAOPrimitives::draw("sphere");
  }



}


void NGLScene::timerEvent(QTimerEvent *_event)
{
 if(_event->timerId()== m_attractorUpdateTimer)
  {

    static float d=0.0f;

    for(auto &a : m_attractors)
    {
      //a=ngl::Random::getRandomPoint(10,10,10);
//      float dt=m_elapsedTimer.elapsed()/60.0f;
//      a.m_x=sinf(dt)*ngl::Random::randomPositiveNumber(60);
//      a.m_y=cosf(dt)*ngl::Random::randomNumber(20);
//      a.m_z=tanf(dt);
      a.m_x = sinf(ngl::radians(d)) * ngl::Random::randomPositiveNumber(20);
      a.m_y = cosf(ngl::radians(d)) * ngl::Random::randomPositiveNumber(20);
      a.m_z = tanf(ngl::radians(d));


      //a.m_z=sinf(dt)*ngl::Random::randomPositiveNumber(10); //0.0f;//tanf(0.5f);
      std::cout<<a<<'\n';
    }
    d+=1.0f;
    glGenBuffers(1, &m_attractorBufferID);
    glBindBuffer(GL_ARRAY_BUFFER, m_attractorBufferID);
    glBufferData(GL_ARRAY_BUFFER, m_attractors.size() * sizeof(ngl::Vec3), &m_attractors[0].m_x, GL_DYNAMIC_COPY);
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
     // m_dt=0.5f;
    break;

    case Qt::Key_Up :
      //m_dt+=0.01f;
    break;
  case Qt::Key_Down :
    //m_dt-=0.01f;
  break;


    default:
      break;
  }
  update();
}
