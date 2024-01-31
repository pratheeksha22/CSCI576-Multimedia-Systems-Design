#include <wx/wx.h>
#include <wx/dcbuffer.h>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

using namespace std;
namespace fs = std::filesystem;

/**
 * Display an image using WxWidgets.
 * https://www.wxwidgets.org/
 */

/** Declarations*/

/**
 * Class that implements wxApp
 */
class MyApp : public wxApp {
 public:
  bool OnInit() override;
  float scale_factor;
};

/**
 * Class that implements wxFrame.
 * This frame serves as the top level window for the program
 */
class MyFrame : public wxFrame {
 public:
  MyFrame(const wxString &title, string imagePath, int antiAlias, int w, float scale_factor);
  
  private:
  void OnPaint(wxPaintEvent &event);
  void OnMouseMove(wxMouseEvent &event);
  void OnKeyUp(wxKeyEvent &event);
  void restoreInImage();
  wxImage inImage;
  wxImage originalImage;
  wxImage curBackupImage;
  wxImage* overlayImage;
  wxScrolledWindow *scrolledWindow;
  int width; //Original width
  int height; //Original Height
  int oWidth; //Downscaled width
  int oHeight; //Downscaled height
  int w; //Overlay window size
  float sf; //scale factor
  unsigned char *inData;
  unsigned char* outData;
};



/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height);
bool isOverlayActive = false;

/** Definitions */

/**
 * Init method for the app.
 * Here we process the command line arguments and
 * instantiate the frame.
 */
bool MyApp::OnInit() {
  wxInitAllImageHandlers();
  wxLog *logger = new wxLogStderr();
  wxLog::SetActiveTarget(logger);

  // deal with command line arguments here
  cout << "Number of command line arguments: " << wxApp::argc << endl;
  if (wxApp::argc != 5) {
    cerr << "The executable should be invoked with exactly one filepath "
            "arguments. Example ./MyImageApplication '../../lake-forest.rgb'"
         << endl;
    exit(1);
  }
  cout << "First argument: " << wxApp::argv[0] << endl;
  cout << "Second argument: " << wxApp::argv[1] << endl;
  cout << "Third argument: " << wxApp::argv[2] << endl;
  cout << "Fourth argument: " << wxApp::argv[3] << endl;
  cout << "Fifth argument: " << wxApp::argv[4] << endl;
  string imagePath = wxApp::argv[1].ToStdString();
  string sf = wxApp::argv[2].ToStdString();
  string A = wxApp::argv[3].ToStdString();
  string w = wxApp::argv[4].ToStdString();
  float scale_factor = stof(sf);
  int antiAlias = stoi(A);
  int overlayWindowSize = stoi(w);

  //cout<<"Scale:"<<scale_factor<<endl;
  MyFrame *frame = new MyFrame("Image Display", imagePath, antiAlias, overlayWindowSize, scale_factor);
  frame->Show(true);

  // return true to continue, false to exit the application
  return true;
}

unsigned char* downScale(unsigned char *inData, int iWidth, int iHeight, float scale_factor){
  
  int oWidth = scale_factor * iWidth;
  int oHeight = scale_factor * iHeight;

  cout<<"Input Width:"<<iWidth<<endl;
  cout<<"Input Height:"<<iHeight<<endl;
  cout<<"Downscaled Width:"<<oWidth<<endl;
  cout<<"Downscaled Height:"<<oHeight<<endl;
  unsigned char* outData = (unsigned char *)malloc(oWidth * oHeight * 3 * sizeof(unsigned char));

  for(int y = 0; y < oHeight; y++){
    for(int x = 0; x < oWidth; x++){
      int iX = x/scale_factor;
      int iY = y/scale_factor;

      if(iX < 0) iX = 0;
      if(iX >= iWidth) iX = iWidth - 1;
      if(iY < 0) iY = 0;
      if(iY >= iHeight) iY = iHeight - 1;

      int outputID = y*oWidth + x;
      int inputID = iY*iWidth + iX;
      int red = inData[3*inputID];
      int green = inData[3*inputID+1];
      int blue = inData[3*inputID+2];

      outData[3*outputID] = red;
      outData[3*outputID+1] = green;
      outData[3*outputID+2] = blue;
      
    }
  }
  return outData;
}

unsigned char* antiAliasing(unsigned char *inData, int iWidth, int iHeight){

  unsigned char* outData = (unsigned char *)malloc(iWidth * iHeight * 3 * sizeof(unsigned char));

  for(int y = 0; y < iHeight; y++){
    for(int x = 0; x < iWidth; x++){
      unsigned int sumR = 0, sumG = 0, sumB = 0;
      for(int dy = -2; dy<=2; dy++){
        for(int dx = -2; dx<=2; dx++){
          int iX = x + dx;
          int iY = y + dy;
          
          if(iX>=0 && iX<iWidth && iY>=0 && iY<iHeight){

            int inputId = iY * iWidth + iX;
            sumR += inData[3*inputId];
            sumG += inData[3*inputId+1];
            sumB += inData[3*inputId+2];
          }
        }
      }

      int outputId = y*iWidth + x;
      outData[3*outputId] = (char)sumR/25;
      outData[3*outputId+1] = (char)sumG/25;
      outData[3*outputId+2] = (char)sumB/25;
    }
  }

  return outData;
}

/**
 * Constructor for the MyFrame class.
 * Here we read the pixel data from the file and set up the scrollable window.
 */
MyFrame::MyFrame(const wxString &title, string imagePath, int antiAlias, int overlayWindowSize, float scale_factor)
    : wxFrame(NULL, wxID_ANY, title) {

  //Default size
  width = 7680;
  height = 4320;
  w = overlayWindowSize;
  sf = scale_factor;
  inData = readImageData(imagePath, width, height);

  oWidth = scale_factor * width;
  oHeight = scale_factor * height;

  // the last argument is static_data, if it is false, after this call the
  // pointer to the data is owned by the wxImage object, which will be
  // responsible for deleting it. So this means that you should not delete the
  // data yourself.
  originalImage.SetData(inData, width, height, false);

  if(antiAlias==1){
    printf("Performing Anti aliasing and downscaling\n");
    unsigned char* antiAliasData = antiAliasing(inData, width, height);
    outData = downScale(antiAliasData, width, height, scale_factor);
    inImage.SetData(outData, oWidth, oHeight, false);
  }
  else{
    printf("Performing downscaling\n");
    outData = downScale(inData, width, height, scale_factor);
    inImage.SetData(outData, oWidth, oHeight, false);
  }

  //Backup image to restore after overlay
  curBackupImage = inImage.Copy();

  // Set up the scrolled window as a child of this frame
  scrolledWindow = new wxScrolledWindow(this, wxID_ANY);
  scrolledWindow->SetScrollbars(10, 10, oWidth, oHeight);
  scrolledWindow->SetVirtualSize(oWidth, oHeight);

  // Bind the paint event to the OnPaint function of the scrolled window
  scrolledWindow->Bind(wxEVT_PAINT, &MyFrame::OnPaint, this);
  scrolledWindow->Bind(wxEVT_MOTION, &MyFrame::OnMouseMove, this);
  scrolledWindow->Bind(wxEVT_KEY_UP, &MyFrame::OnKeyUp, this);
  // Set the frame size
  SetClientSize(oWidth, oHeight);

  // Set the frame background color
  SetBackgroundColour(*wxBLACK);


}

/* Utility function to restore image after overlay */
void MyFrame::restoreInImage() {
    inImage = curBackupImage.Copy();
    isOverlayActive = false;
    Refresh(); // Refresh the display to show the original image
}


/* Key up handler that restores image in scrolledWindow when Ctrl key is released */
void MyFrame::OnKeyUp(wxKeyEvent &event){
  if (event.GetKeyCode() == WXK_CONTROL) {
        // Control key is released, restore the original inImage
        if (isOverlayActive) {
            restoreInImage();
        }
    }
    event.Skip();
}


/*  OnMuseMove handler that captures mouse motion and overlays original image in the corresponding window region of size w
    GetX() and GetY() are inherited from MouseState */
void MyFrame::OnMouseMove(wxMouseEvent& event){
  int startX, startY, endX, endY;
  int *store;
 
  if(event.GetModifiers() == wxMOD_CONTROL){
    if (isOverlayActive) {
          restoreInImage();
    }

    int mouseX = event.GetX();
    int mouseY = event.GetY();
    int origX = mouseX / sf;
    int origY = mouseY / sf;
    startX = mouseX - w / 2;
    startY = mouseY - w / 2;
    endX = mouseX + w / 2;
    endY = mouseY + w / 2;

    if(startX<0) startX = 0;
    if(startY<0) startY = 0;
    if(endX>=oWidth) endX = oWidth - 1;
    if(endY>=oHeight) endY = oHeight - 1;

    for (int y = startY; y < endY; y++) { 
      for (int x = startX; x < endX; x++) { 
        int newOrigX = origX + x - mouseX; 
        int newOrigY = origY + y - mouseY; 
        int red = originalImage.GetRed(newOrigX, newOrigY);
        int green = originalImage.GetGreen(newOrigX, newOrigY);
        int blue = originalImage.GetBlue(newOrigX, newOrigY);
        inImage.SetRGB(x, y, red, green , blue);
        
      } 
    }
    isOverlayActive = true;

    //Refresh to show the overlay
    Refresh();
  }
}


/**
 * The OnPaint handler that paints the UI.
 * Here we paint the image pixels into the scrollable window.
 */
void MyFrame::OnPaint(wxPaintEvent &event) {
  wxBufferedPaintDC dc(scrolledWindow);
  scrolledWindow->DoPrepareDC(dc);

  wxBitmap inImageBitmap = wxBitmap(inImage);
  dc.DrawBitmap(inImageBitmap, 0, 0, false);
}

/** Utility function to read image data */
unsigned char *readImageData(string imagePath, int width, int height) {

  // Open the file in binary mode
  ifstream inputFile(imagePath, ios::binary);

  if (!inputFile.is_open()) {
    cerr << "Error Opening File for Reading" << endl;
    exit(1);
  }

  // Create and populate RGB buffers
  vector<char> Rbuf(width * height);
  vector<char> Gbuf(width * height);
  vector<char> Bbuf(width * height);

  /**
   * The input RGB file is formatted as RRRR.....GGGG....BBBB.
   * i.e the R values of all the pixels followed by the G values
   * of all the pixels followed by the B values of all pixels.
   * Hence we read the data in that order.
   */

  inputFile.read(Rbuf.data(), width * height);
  inputFile.read(Gbuf.data(), width * height);
  inputFile.read(Bbuf.data(), width * height);

  inputFile.close();

  /**
   * Allocate a buffer to store the pixel values
   * The data must be allocated with malloc(), NOT with operator new. wxWidgets
   * library requires this.
   */
  unsigned char *inData =
      (unsigned char *)malloc(width * height * 3 * sizeof(unsigned char));
      
  for (int i = 0; i < height * width; i++) {
    // We populate RGB values of each pixel in that order
    // RGB.RGB.RGB and so on for all pixels
    inData[3 * i] = Rbuf[i];
    inData[3 * i + 1] = Gbuf[i];
    inData[3 * i + 2] = Bbuf[i];
  }

  return inData;
}

wxIMPLEMENT_APP(MyApp);