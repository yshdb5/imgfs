# ImgFS - Image File System

**CS202 Computer Systems Project 2024**

A sophisticated image-oriented file system with both command-line and web interfaces, featuring multi-resolution storage, deduplication, and RESTful API access.

## 🎯 Overview

ImgFS is a custom file system designed specifically for efficient image storage and management. It supports multiple image resolutions (thumbnail, small, original), automatic deduplication using SHA-256 hashes, and provides both command-line tools and a web server interface for image operations.

## ✨ Features

### Core Functionality
- **Multi-resolution Storage**: Automatic generation and storage of thumbnail, small, and original image sizes
- **Image Deduplication**: SHA-256-based duplicate detection to save storage space
- **Metadata Management**: Structured headers with comprehensive image metadata
- **CRUD Operations**: Create, read, update, and delete images with full metadata tracking

### Interfaces
- **Command-line Tool** (`imgfscmd`): Full-featured CLI for all imgFS operations
- **HTTP Web Server** (`imgfs_server`): RESTful API with JSON responses
- **Socket Layer**: Custom networking implementation for web service

### Technical Features
- **Memory Safety**: Built with AddressSanitizer integration
- **Image Processing**: VIPS library integration for high-performance image operations
- **Error Handling**: Comprehensive error codes and messages
- **Extensive Testing**: Unit tests and end-to-end test suites

## 🏗️ Architecture

```
┌─────────────────┐  ┌─────────────────┐
│   Web Client    │  │   CLI Client    │
└─────────┬───────┘  └─────────┬───────┘
          │                    │
          │ HTTP/JSON          │ Command Line
          │                    │
┌─────────▼───────┐  ┌─────────▼───────┐
│  HTTP Server    │  │   imgfscmd      │
│  (Port 8000)    │  │   Interface     │
└─────────┬───────┘  └─────────┬───────┘
          │                    │
          └──────────┬─────────┘
                     │
           ┌─────────▼───────┐
           │   ImgFS Core    │
           │   Engine        │
           └─────────┬───────┘
                     │
           ┌─────────▼───────┐
           │  File System    │
           │  (.imgfs files) │
           └─────────────────┘
```

## 🔧 Dependencies

### Required Libraries
- **VIPS**: Image processing library
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libvips-dev
  
  # macOS
  brew install vips
  ```

- **JSON-C**: JSON parsing and generation
  ```bash
  # Ubuntu/Debian
  sudo apt-get install libjson-c-dev
  
  # macOS
  brew install json-c
  ```

- **OpenSSL**: For SHA-256 hashing (usually pre-installed)

### Build Tools
- GCC or Clang compiler
- Make
- pkg-config

## 🚀 Building

### Basic Build
```bash
cd done/
make
```

### Available Targets
```bash
make imgfscmd      # Build command-line tool
make imgfs_server  # Build web server
make all          # Build everything
make clean        # Clean build artifacts
make style        # Format code with astyle
make doc          # Generate documentation
```

### Build Configurations
The project builds with AddressSanitizer by default for enhanced debugging:
```bash
# Standard build with sanitizer
make

# Release build (if needed)
make CPPFLAGS= LDFLAGS=
```

## 📖 Usage

### Command-Line Interface

#### Create a new ImgFS
```bash
./imgfscmd create myimages.imgfs
```

#### List all images
```bash
./imgfscmd list myimages.imgfs
```

#### Insert an image
```bash
./imgfscmd insert myimages.imgfs vacation.jpg beach_sunset
```

#### Read an image (with optional resolution)
```bash
# Read original size
./imgfscmd read myimages.imgfs beach_sunset vacation_orig.jpg

# Read thumbnail
./imgfscmd read myimages.imgfs beach_sunset vacation_thumb.jpg thumb

# Read small resolution
./imgfscmd read myimages.imgfs beach_sunset vacation_small.jpg small
```

#### Delete an image
```bash
./imgfscmd delete myimages.imgfs beach_sunset
```

#### Help
```bash
./imgfscmd help
```

### Web Server Interface

#### Start the server
```bash
./imgfs_server myimages.imgfs [port]
# Default port: 8000
```

#### Web API Endpoints

| Method | Endpoint | Description | Parameters |
|--------|----------|-------------|------------|
| `GET` | `/` | Serve main HTML page | - |
| `GET` | `/imgfs/list` | List all images (JSON) | - |
| `GET` | `/imgfs/read` | Read image | `img_id`, `res` (optional) |
| `POST` | `/imgfs/insert` | Insert new image | `name`, image file |
| `POST` | `/imgfs/delete` | Delete image | `img_id` |

#### Example API Usage
```bash
# List images
curl http://localhost:8000/imgfs/list

# Read thumbnail
curl http://localhost:8000/imgfs/read?img_id=beach_sunset&res=thumb

# Insert image (using form data)
curl -X POST -F "name=my_photo" -F "file=@photo.jpg" \
     http://localhost:8000/imgfs/insert

# Delete image
curl -X POST -d "img_id=my_photo" \
     http://localhost:8000/imgfs/delete
```

## 🗂️ File Format

### ImgFS File Structure
```
┌─────────────────┐
│  ImgFS Header   │  Fixed size header with metadata
├─────────────────┤
│  Image Metadata │  Array of image descriptors
│     Array       │  (size = max_files)
├─────────────────┤
│                 │
│  Image Data     │  Raw image data for all resolutions
│     Blob        │  (offset-addressed)
│                 │
└─────────────────┘
```

### Header Structure
- **Name**: ImgFS identifier (max 31 chars)
- **Version**: Format version number
- **File Counts**: Current and maximum number of images
- **Resolution Settings**: Thumbnail and small image dimensions
- **Metadata Array**: Fixed-size array of image descriptors

### Image Metadata
- **ID**: Unique image identifier (max 127 chars)
- **SHA-256**: Content hash for deduplication
- **Dimensions**: Original image resolution
- **Sizes**: File sizes for each resolution
- **Offsets**: File positions for each resolution variant

## 🧪 Testing

### Unit Tests
```bash
cd tests/unit/
make
./unit-tests
```

### End-to-End Tests
```bash
cd tests/end-to-end/
# Requires Robot Framework
robot week*.robot
```

### Integration Testing
```bash
# Test with provided sample data
./imgfscmd list tests/data/test02.imgfs
./imgfs_server tests/data/test02.imgfs 8080
```

## 📁 Project Structure

```
done/
├── imgfscmd.c              # Main CLI application
├── imgfs_server.c          # Web server main
├── imgfs.h                 # Core data structures
├── imgfs_tools.c           # Utility functions
├── imgfscmd_functions.c    # CLI command implementations
├── imgfs_server_service.c  # Web service logic
├── imgfs_create.c          # Create operations
├── imgfs_list.c           # List operations  
├── imgfs_read.c           # Read operations
├── imgfs_insert.c         # Insert operations
├── imgfs_delete.c         # Delete operations
├── image_content.c        # Image processing
├── image_dedup.c          # Deduplication logic
├── http_prot.c            # HTTP protocol handling
├── http_net.c             # HTTP networking
├── socket_layer.c         # Socket abstraction
├── error.c                # Error handling
├── util.c                 # General utilities
└── tests/                 # Test suites
```

## 🔍 Technical Details

### Image Resolution Handling
- **Original**: Stored as-is from source file
- **Small**: Resized version for quick preview
- **Thumbnail**: Smallest version for listings

### Deduplication Algorithm
1. Calculate SHA-256 hash of original image
2. Search existing metadata for matching hash
3. If found, create new metadata entry pointing to existing data
4. If not found, store new image data and create metadata

### Memory Management
- Dynamic metadata array allocation
- Proper resource cleanup on errors
- AddressSanitizer integration for leak detection

### Error Handling
Comprehensive error code system covering:
- File system errors
- Network errors  
- Image processing errors
- Invalid arguments
- Memory allocation failures

## 📄 License

This project is open source and available under the [MIT License](LICENSE).
