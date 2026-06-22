import os

if "__main__" in __name__:
    print("--- RUNNING IN TEST MODE ---")
    class MockEnv:
        def get(self, key, default=None):
            if key == "PROJECT_DIR":
                return os.getcwd() # Assumes you run it from your project root
            return default
        def Append(self, BUILD_FLAGS=None):
            print(f"[MOCK BUILD_FLAGS APPEND]: {BUILD_FLAGS}")
            
    env = MockEnv()
else:
    # This executes normally when called by PlatformIO
    Import("env")

# Path to your .env file
env_file = os.path.join(env.get("PROJECT_DIR"), ".env")

if os.path.exists(env_file):
    with open(env_file, "r") as f:
        for line in f:
            # Clean up line and skip comments/empty lines
            line = line.strip()
            if not line or line.startswith("#"):
                continue
                
            # Split into key and value
            if "=" in line:
                key, val = line.split("=", 1)
                key = key.strip()
                val = val.strip().strip('"').strip("'")

                val = val.replace("$", "$$")

                val = val.replace('"', '\\"')
                
                # Inject into compiler flags with escaped quotes for C++ string literals
                env.Append(BUILD_FLAGS=[f'-D {key}=\'"{val}"\''])
                #env.Append(BUILD_FLAGS=[("-D", f'{key}="{val}"')])
else:
    print(f"Warning: .env file not found at {env_file}")