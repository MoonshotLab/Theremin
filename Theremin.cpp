#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/GenNode.h"
#include "cinder/audio/GainNode.h"

using namespace ci;
using namespace ci::app;
using namespace std;

#include "Kinect2.h"

class Theremin : public ci::app::AppBasic
{
public:
	void setup();
	void prepareSettings( ci::app::AppBasic::Settings* settings );
	void update();
    void draw();

private:
	// Kinect Stuff
	Kinect2::DeviceRef mDevice;
    Kinect2::BodyFrame mBodyFrame;

	// Audio references
	audio::GenNodeRef mLeftHandGen;
	audio::GainNodeRef mLeftHandGain;
	audio::GenNodeRef mRightHandGen;
	audio::GainNodeRef mRightHandGain;
	
	// Keep track of the ID for a single body
    UINT64 mTrackingID;

	// Values for the position of left and right hand
    Vec3f mLeftHand;
	Vec3f mRightHand;

	// Is our hand open, closed, pointing, lasso?
	bool mLeftHandClosed;
	bool mRightHandClosed;

	// Keep track of how many frames in a row are not closed
	// so we get a smoother signal
	int mLeftHandFramesNotClosed;
	int mRightHandFramesNotClosed;
};

void Theremin::prepareSettings( Settings* settings )
{
	settings->prepareWindow( Window::Format().size( 800, 800 ).title( "Theremin" ) );
	settings->setFrameRate( 30.0f );
}

void Theremin::setup()
{
	// Create the audio references
	// Using Different node types prevents the sounds from cancelling
	// each other out
	auto ctx = audio::master();
	mLeftHandGen = ctx->makeNode(new audio::GenPulseNode);
	mLeftHandGain = ctx->makeNode(new audio::GainNode);
	mRightHandGen = ctx->makeNode(new audio::GenSineNode);
	mRightHandGain = ctx->makeNode(new audio::GainNode);

	mLeftHandGen >> mLeftHandGain >> ctx->getOutput();
	mRightHandGen >> mRightHandGain >> ctx->getOutput();

	mLeftHandGen->enable();
	mRightHandGen->enable();
	ctx->enable();

    // Setup the Kinect and track the body frame
	mDevice = Kinect2::Device::create();
	mDevice->start();
	mDevice->connectBodyEventHandler( [ & ]( const Kinect2::BodyFrame frame )
	{
		mBodyFrame = frame;
	} );

	// User details
    mTrackingID					= 0;
    mLeftHand					= Vec3f(0, 0, 0);
	mRightHand					= Vec3f(0, 0, 0);
	mLeftHandClosed				= false;
	mRightHandClosed			= false;
	mLeftHandFramesNotClosed	= 0;
	mRightHandFramesNotClosed	= 0;
}

void Theremin::update()
{
    // Get the body frame from the kinect
    const std::vector<Kinect2::Body> svBodies = mBodyFrame.getBodies();

    // If not tracking a body, get one
	if (mTrackingID == 0)
    {
        for (const Kinect2::Body& body : svBodies) {
            if (body.isTracked()) {
                mTrackingID = body.getId();
            }
        }
    }

	// If we have somebody...
	bool isTracking = false;
	for (const Kinect2::Body& body : svBodies) {
		if (body.isTracked() && mTrackingID == body.getId()) {
			isTracking = true;

			// Get the joints and hands
			const map<JointType, Kinect2::Body::Joint> joints = body.getJointMap();
			auto leftHandJoint = joints.find(JointType::JointType_HandLeft);
			auto rightHandJoint = joints.find(JointType::JointType_HandRight);

			// Save the hand positions
			mLeftHand = leftHandJoint->second.getPosition();
			mRightHand = rightHandJoint->second.getPosition();

			// If the hand appears to be closed, verify it's been closed for at
			// least 3 frames before saying it's actually closed. This makes the
			// sound less choppy since the Kinect tends to miss a frame once in
			// a while for a hand state
			if (body.getLeftHandState() != 3){
				mLeftHandFramesNotClosed++;
				if (mLeftHandFramesNotClosed > 5){
					mLeftHandClosed = false;
					mLeftHandFramesNotClosed = 0;
				}
			} else{
				mLeftHandClosed = true;
				mLeftHandFramesNotClosed = 0;
			}

			if (body.getRightHandState() != 3){
				mRightHandFramesNotClosed++;
				if (mRightHandFramesNotClosed > 5){
					mRightHandClosed = false;
					mRightHandFramesNotClosed = 0;
				}
			}
			else{
				mRightHandClosed = true;
				mRightHandFramesNotClosed = 0;
			}

			// We're only using one body, so stop
			break;
		}
	}

	// If nobody's around, reset
	if (mTrackingID == 0 || !isTracking)
	{
		mTrackingID = 0;
		return;
	}
}

void Theremin::draw()
{
	// Draw some visuals to represent our sounds
	gl::clear();
	gl::drawSolidRect(ci::Rectf(0.0f, 0.0f, (float)getWindowWidth()/2, (float)getWindowHeight()));
	float RColor = ci::lmap(mRightHand.y, -1.0f, 1.0f, 0.0f, 1.0f);
	gl::color(0.0f, 0.0f, RColor);
	gl::drawSolidRect(ci::Rectf((float)getWindowHeight()/2, 0, (float)getWindowHeight(), (float)getWindowHeight()));
	float LColor = ci::lmap(mLeftHand.y, -1.0f, 1.0f, 0.0f, 1.0f);
	gl::color(LColor, 0.0f, 0.0f);

	// Play sounds if the hands are closed, other wise mute
	if (mLeftHandClosed){
		float volume = ci::lmap(mLeftHand.z, 1.0f, .5f, 0.0f, 1.0f);
		float frequency = ci::lmap(mLeftHand.y, -.5f, .5f, 0.0f, 700.0f);

		mLeftHandGain->setValue(volume);
		mLeftHandGen->setFreq(frequency);
	} else{
		mLeftHandGain->setValue(0.0f);
	}

	if (mRightHandClosed){
		float volume = ci::lmap(mRightHand.z, 1.0f, .5f, 0.0f, 1.0f);
		float frequency = ci::lmap(mRightHand.y, -.5f, .5f, 0.0f, 700.0f);

		mRightHandGain->setValue(volume);
		mRightHandGen->setFreq(frequency);
	}
	else{
		mRightHandGain->setValue(0.0f);
	}
}

CINDER_APP_BASIC(Theremin, RendererGl)
