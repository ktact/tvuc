#!/bin/bash

# README Language Switcher for Vhost-User Client Project
# Created by Claude Code

usage() {
    echo "README Language Switcher"
    echo ""
    echo "Usage: $0 [LANGUAGE]"
    echo ""
    echo "Languages:"
    echo "  en, english    - Switch to English README"
    echo "  ja, japanese   - Switch to Japanese README"
    echo "  hub, default   - Use language selection hub (default)"
    echo "  status         - Show current README status"
    echo ""
    echo "Examples:"
    echo "  $0 en          # Switch to English"
    echo "  $0 ja          # Switch to Japanese"
    echo "  $0 hub         # Restore language selection hub"
    echo "  $0 status      # Show current language"
}

show_status() {
    if [ -f "README.md" ]; then
        echo "Current README.md status:"
        
        # Check which version is currently active
        if grep -q "Choose Your Language" README.md; then
            echo "  📋 Language selection hub (default)"
        elif grep -q "Language.*English.*日本語" README.md; then
            if grep -q "\*\*English\*\*" README.md; then
                echo "  🇺🇸 English version active"
            elif grep -q "\*\*日本語\*\*" README.md; then
                echo "  🇯🇵 Japanese version active"
            else
                echo "  ❓ Language-aware but unknown version"
            fi
        else
            echo "  ❓ Unknown README version"
        fi
        
        echo "  📊 File size: $(wc -c < README.md) bytes"
        echo "  📝 Lines: $(wc -l < README.md) lines"
    else
        echo "❌ README.md not found"
    fi
    
    echo ""
    echo "Available versions:"
    [ -f "README.en.md" ] && echo "  ✅ English (README.en.md)" || echo "  ❌ English (README.en.md)"
    [ -f "README.ja.md" ] && echo "  ✅ Japanese (README.ja.md)" || echo "  ❌ Japanese (README.ja.md)"
}

switch_to_language() {
    local lang="$1"
    local source_file=""
    local lang_name=""
    
    case "$lang" in
        en|english)
            source_file="README.en.md"
            lang_name="English"
            ;;
        ja|japanese)
            source_file="README.ja.md"
            lang_name="Japanese"
            ;;
        hub|default)
            # Restore the hub version
            if [ -f "README.hub.backup" ]; then
                cp README.hub.backup README.md
                echo "✅ Restored language selection hub"
                return 0
            else
                echo "❌ Hub backup not found. Cannot restore language selection."
                return 1
            fi
            ;;
        *)
            echo "❌ Unknown language: $lang"
            usage
            return 1
            ;;
    esac
    
    if [ ! -f "$source_file" ]; then
        echo "❌ $lang_name README file not found: $source_file"
        return 1
    fi
    
    # Backup current README if it's the hub version
    if grep -q "Choose Your Language" README.md 2>/dev/null; then
        cp README.md README.hub.backup
    fi
    
    # Switch to the requested language
    cp "$source_file" README.md
    echo "✅ Switched README to $lang_name"
    echo "   Source: $source_file"
    echo "   To restore language selection: $0 hub"
}

# Main execution
case "${1:-status}" in
    en|english|ja|japanese|hub|default)
        switch_to_language "$1"
        ;;
    status)
        show_status
        ;;
    help|-h|--help)
        usage
        ;;
    *)
        echo "❌ Unknown command: $1"
        echo ""
        usage
        exit 1
        ;;
esac