// cat > test_simple.cpp << 'EOF'
int main() {
    int x = 42;     // Should be a STORE
    int y = x;      // Should be a LOAD and STORE
    return y;
}
// EOF

// ./instrument.sh test_simple.cpp