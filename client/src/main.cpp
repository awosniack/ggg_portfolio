#include "Client.hpp"
#include "ClientInventory.hpp"
#include "ItemIcons.h"
#include "raylib.h"
#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unordered_map>

const int SCREEN_WIDTH = 950;
const int SCREEN_HEIGHT = 550;
const int SLOT_SIZE = 35;
const int SLOT_PADDING = 0;

// shared stash on the left
const int STASH_OFFSET_X = 50;
const int STASH_OFFSET_Y = 100;

// player inventory to the right of shared stash
const int INVENTORY_OFFSET_X = STASH_OFFSET_X + (12 * (SLOT_SIZE + SLOT_PADDING)) + 30;
// align bottom of personal inventory (5 tall) with bottom of shared stash (12 tall)
const int INVENTORY_OFFSET_Y = STASH_OFFSET_Y + (12 * (SLOT_SIZE + SLOT_PADDING)) - (5 * (SLOT_SIZE + SLOT_PADDING));

const Color BACKGROUND_COLOR = BLACK;
const Color SLOT_BORDER_COLOR = Color{28, 25, 18, 255};
const Color HOVER_BACKGROUND_COLOR = GRAY;
const Color HOVER_BORDER_COLOR = WHITE;

struct DragState
{
    bool isDragging;
    inventory::InventoryType sourceInventory;
    inventory::GridPosition sourcePos;
    std::shared_ptr<inventory::Item> draggedItem;
    uint32_t stackCount;
    int mouseOffsetX;
    int mouseOffsetY;

    DragState() : isDragging(false), sourceInventory(inventory::InventoryType::PERSONAL),
                  sourcePos(), draggedItem(nullptr), stackCount(0),
                  mouseOffsetX(0), mouseOffsetY(0) {}
};

struct SplitDialogState
{
    bool active;
    inventory::InventoryType invType;
    inventory::GridPosition sourcePos;
    std::shared_ptr<inventory::Item> item;
    uint32_t maxAmount;
    char inputBuffer[16];

    SplitDialogState() : active(false), invType(inventory::InventoryType::PERSONAL),
                         sourcePos(), item(nullptr), maxAmount(0)
    {
        inputBuffer[0] = '\0';
    }
};

// function to convert screen coordinates to inventory grid position
inventory::GridPosition screenToInventoryGrid(int screenX, int screenY, int offsetX, int offsetY, int *outPosX = nullptr, int *outPosY = nullptr)
{
    int gridX = (screenX - offsetX) / (SLOT_SIZE + SLOT_PADDING);
    int gridY = (screenY - offsetY) / (SLOT_SIZE + SLOT_PADDING);

    if (outPosX)
        *outPosX = offsetX + gridX * (SLOT_SIZE + SLOT_PADDING);
    if (outPosY)
        *outPosY = offsetY + gridY * (SLOT_SIZE + SLOT_PADDING);

    return inventory::GridPosition(gridX, gridY);
}

// check if mouse is over inventory area
bool isMouseOverInventory(int mouseX, int mouseY, int width, int height, int offsetX, int offsetY)
{
    int invWidth = width * (SLOT_SIZE + SLOT_PADDING);
    int invHeight = height * (SLOT_SIZE + SLOT_PADDING);
    return mouseX >= offsetX && mouseX < offsetX + invWidth &&
           mouseY >= offsetY && mouseY < offsetY + invHeight;
}

void drawInventoryGrid(int width, int height, int offsetX, int offsetY, const inventory::GridPosition *hoveredSlot = nullptr)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            int posX = offsetX + x * (SLOT_SIZE + SLOT_PADDING);
            int posY = offsetY + y * (SLOT_SIZE + SLOT_PADDING);
            Color bgColor = BACKGROUND_COLOR;
            Color borderColor = SLOT_BORDER_COLOR;
            // highlighting hovered slot
            if (hoveredSlot && hoveredSlot->x == x && hoveredSlot->y == y)
            {
                bgColor = HOVER_BACKGROUND_COLOR;
                borderColor = HOVER_BORDER_COLOR;
            }
            // sslot background
            DrawRectangle(posX, posY, SLOT_SIZE, SLOT_SIZE, bgColor);
            // slot border
            DrawRectangleLines(posX, posY, SLOT_SIZE, SLOT_SIZE, borderColor);
        }
    }
}

void drawInventoryItems(const std::shared_ptr<inventory::ClientInventory> &inv, int offsetX, int offsetY,
                        const DragState *dragState = nullptr, inventory::InventoryType invType = inventory::InventoryType::PERSONAL,
                        std::unordered_map<std::string, Texture2D> *iconCache = nullptr)
{
    if (!inv)
        return;

    auto items = inv->getAllItems();
    for (const auto &slot : items)
    {
        if (!slot.item)
            continue;

        // skip the drawing if this item is being dragged from this inventory
        if (dragState && dragState->isDragging &&
            dragState->sourceInventory == invType &&
            slot.position.x == dragState->sourcePos.x &&
            slot.position.y == dragState->sourcePos.y)
        {
            continue;
        }

        int posX = offsetX + slot.position.x * (SLOT_SIZE + SLOT_PADDING);
        int posY = offsetY + slot.position.y * (SLOT_SIZE + SLOT_PADDING);

        auto size = slot.item->getSize();
        int itemWidth = size.width * SLOT_SIZE + (size.width - 1) * SLOT_PADDING;
        int itemHeight = size.height * SLOT_SIZE + (size.height - 1) * SLOT_PADDING;

        // load and draw icon texture
        bool iconDrawn = false;
        if (iconCache)
        {
            std::string iconPath = getItemIconPath(slot.item->getId());

            if (!iconPath.empty())
            {
                // check if texture is on cache
                if (iconCache->find(iconPath) == iconCache->end())
                {
                    // load texture
                    Texture2D texture = LoadTexture(iconPath.c_str());
                    if (texture.id != 0)
                    {
                        (*iconCache)[iconPath] = texture;
                        std::cout << "Loaded texture: " << iconPath << " (size: " << texture.width << "x" << texture.height << ")" << std::endl;
                    }
                    else
                    {
                        std::cout << "Failed to load texture: " << iconPath << std::endl;
                        // store empty texture to avoid retrying
                        (*iconCache)[iconPath] = Texture2D{0, 0, 0, 0, 0};
                    }
                }

                // draw texture if available
                if (iconCache->find(iconPath) != iconCache->end())
                {
                    Texture2D &texture = (*iconCache)[iconPath];
                    if (texture.id != 0)
                    {
                        // the faint blue background behind the icon
                        DrawRectangle(posX + 2, posY + 2, itemWidth - 4, itemHeight - 4, {0, 0, 27, 180});

                        // drawing the icon asset
                        Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
                        Rectangle dest = {(float)(posX + 2), (float)(posY + 2),
                                          (float)(itemWidth - 4), (float)(itemHeight - 4)};
                        DrawTexturePro(texture, source, dest, Vector2{0, 0}, 0.0f, WHITE);
                        iconDrawn = true;
                    }
                }
            }
            else
            {
                std::cout << "Item " << slot.item->getName() << " has empty image path" << std::endl;
            }
        }

        // if no icon - draw a colored rectangle depending on stack size
        if (!iconDrawn)
        {
            Color itemColor = BLUE;
            if (size.width * size.height >= 4)
            {
                itemColor = PURPLE;
            }
            else if (slot.item->getStackLimit() > 9)
            {
                itemColor = GOLD;
            }

            DrawRectangle(posX + 2, posY + 2, itemWidth - 4, itemHeight - 4, itemColor);
        }

        DrawRectangleLines(posX + 1, posY + 1, itemWidth - 2, itemHeight - 2, WHITE);

        // abbreviate item name if too long
        std::string name = slot.item->getName();
        if (name.length() > 10)
        {
            name = name.substr(0, 9) + ".";
        }

        // draw item name (optional - commented out for cleaner UI)

        // int textPosX = posX + 4;
        // int textPosY = posY + 4;
        // DrawText(name.c_str(), textPosX, textPosY, 10, WHITE);

        // only draw stack count if > 1
        if (slot.stackCount > 1)
        {
            std::string countStr = std::to_string(slot.stackCount);
            int countTextWidth = MeasureText(countStr.c_str(), 14);
            DrawText(countStr.c_str(),
                     posX + itemWidth - countTextWidth - 4,
                     posY + itemHeight - 18,
                     14, YELLOW);
        }
    }
}

int main(int argc, char *argv[])
{
    std::cout << "Inventory System - Client" << std::endl;

    // default server settings
    std::string host = "127.0.0.1";
    int port = 7777;

    // get server info from args if provided
    if (argc > 1)
    {
        host = argv[1];
    }
    if (argc > 2)
    {
        port = std::atoi(argv[2]);
    }

    // get username
    std::string username;
    std::cout << "Enter your username: ";
    std::getline(std::cin, username);

    if (username.empty())
    {
        std::cerr << "Username cannot be empty" << std::endl;
        return 1;
    }

    // connect client
    inventory::Client client;

    if (!client.connect(host.c_str(), port, username))
    {
        std::cerr << "Failed to connect to server" << std::endl;
        return 1;
    }

    std::cout << "Connected to server as " << client.getUsername() << std::endl;

    // initialize Raylib
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Inventory System");
    SetTargetFPS(60);

    std::cout << "Raylib window initialized" << std::endl;

    DragState dragState;
    SplitDialogState splitDialog;
    inventory::GridPosition hoveredSlot(-1, -1);
    int currentStashIndex = 0; // current selected shared stash

    // icon texture cache
    std::unordered_map<std::string, Texture2D> iconCache;

    // main game loop
    while (client.isConnected())
    {
        // only check for window close if split dialog is not active
        if (WindowShouldClose() && !splitDialog.active)
        {
            break;
        }

        // get mouse position
        Vector2 mousePos = GetMousePosition();
        int mouseX = static_cast<int>(mousePos.x);
        int mouseY = static_cast<int>(mousePos.y);

        // handle stash tab switching (1, 2, 3 keys) - but not during split dialog
        if (!splitDialog.active)
        {
            if (IsKeyPressed(KEY_ONE))
                currentStashIndex = 0;
            if (IsKeyPressed(KEY_TWO))
                currentStashIndex = 1;
            if (IsKeyPressed(KEY_THREE))
                currentStashIndex = 2;
        }

        // which inventory mouse is over
        bool mouseOverInventory = isMouseOverInventory(mouseX, mouseY, 12, 5,
                                                       INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y);
        bool mouseOverStash = isMouseOverInventory(mouseX, mouseY, 12, 12,
                                                   STASH_OFFSET_X, STASH_OFFSET_Y);

        // updating hovered slot
        if (mouseOverInventory && !dragState.isDragging && !splitDialog.active)
        {
            hoveredSlot = screenToInventoryGrid(mouseX, mouseY, INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y);
        }
        else if (!mouseOverInventory)
        {
            hoveredSlot = inventory::GridPosition(-1, -1);
        }

        // handling right-click for stack splitting (only in personal inventory)
        if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && !dragState.isDragging && !splitDialog.active)
        {
            if (mouseOverInventory)
            {
                auto personalInv = client.getPersonalInventory();
                if (personalInv)
                {
                    inventory::GridPosition clickedPos = screenToInventoryGrid(mouseX, mouseY,
                                                                               INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y);
                    const auto *slot = personalInv->getSlot(clickedPos.x, clickedPos.y);

                    if (slot && !slot->isEmpty() && slot->stackCount > 1)
                    {
                        splitDialog.active = true;
                        splitDialog.invType = inventory::InventoryType::PERSONAL;
                        splitDialog.sourcePos = clickedPos;
                        splitDialog.item = slot->item;
                        splitDialog.maxAmount = slot->stackCount - 1; // can't split all
                        snprintf(splitDialog.inputBuffer, sizeof(splitDialog.inputBuffer), "%d",
                                 (slot->stackCount + 1) / 2); // default to half
                    }
                }
            }
        }

        // handle mouse input - start dragging
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !dragState.isDragging && !splitDialog.active)
        {
            if (mouseOverInventory)
            {
                // dragging from personal inventory
                auto personalInv = client.getPersonalInventory();
                if (personalInv)
                {
                    inventory::GridPosition clickedPos = screenToInventoryGrid(mouseX, mouseY,
                                                                               INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y);
                    const auto *slot = personalInv->getSlot(clickedPos.x, clickedPos.y);

                    std::cout << "Clicked inventory at (" << clickedPos.x << ", " << clickedPos.y << ")"
                              << " slot=" << (slot ? "exists" : "null")
                              << " empty=" << (slot ? (slot->isEmpty() ? "yes" : "no") : "N/A") << std::endl;

                    if (slot && !slot->isEmpty())
                    {
                        dragState.isDragging = true;
                        dragState.sourceInventory = inventory::InventoryType::PERSONAL;
                        dragState.sourcePos = clickedPos;
                        dragState.draggedItem = slot->item;
                        dragState.stackCount = slot->stackCount;

                        // calculate the mouse offset within the item
                        int slotPosX = INVENTORY_OFFSET_X + clickedPos.x * (SLOT_SIZE + SLOT_PADDING);
                        int slotPosY = INVENTORY_OFFSET_Y + clickedPos.y * (SLOT_SIZE + SLOT_PADDING);
                        dragState.mouseOffsetX = mouseX - slotPosX;
                        dragState.mouseOffsetY = mouseY - slotPosY;

                        std::cout << "Started dragging " << dragState.draggedItem->getName()
                                  << " from personal inventory (" << clickedPos.x << ", " << clickedPos.y << ")" << std::endl;
                    }
                }
            }
            else if (mouseOverStash)
            {
                // dragging from the shared stash
                auto sharedStash = client.getSharedStash(currentStashIndex);
                if (sharedStash)
                {
                    inventory::GridPosition clickedPos = screenToInventoryGrid(mouseX, mouseY,
                                                                               STASH_OFFSET_X, STASH_OFFSET_Y);
                    const auto *slot = sharedStash->getSlot(clickedPos.x, clickedPos.y);

                    if (slot && !slot->isEmpty())
                    {
                        dragState.isDragging = true;
                        dragState.sourceInventory = static_cast<inventory::InventoryType>(
                            static_cast<int>(inventory::InventoryType::SHARED_STASH_1) + currentStashIndex);
                        dragState.sourcePos = clickedPos;
                        dragState.draggedItem = slot->item;
                        dragState.stackCount = slot->stackCount;

                        // calculate mouse offset within the item
                        int slotPosX = STASH_OFFSET_X + clickedPos.x * (SLOT_SIZE + SLOT_PADDING);
                        int slotPosY = STASH_OFFSET_Y + clickedPos.y * (SLOT_SIZE + SLOT_PADDING);
                        dragState.mouseOffsetX = mouseX - slotPosX;
                        dragState.mouseOffsetY = mouseY - slotPosY;

                        std::cout << "Started dragging " << dragState.draggedItem->getName()
                                  << " from shared stash (" << clickedPos.x << ", " << clickedPos.y << ")" << std::endl;
                    }
                }
            }
        }

        // handle mouse input - stop dragging
        if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && dragState.isDragging)
        {
            inventory::InventoryType destInventory = inventory::InventoryType::PERSONAL;
            inventory::GridPosition targetPos;
            bool validDrop = false;

            if (mouseOverInventory)
            {
                targetPos = screenToInventoryGrid(mouseX, mouseY, INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y);
                destInventory = inventory::InventoryType::PERSONAL;
                validDrop = true;
            }
            else if (mouseOverStash)
            {
                targetPos = screenToInventoryGrid(mouseX, mouseY, STASH_OFFSET_X, STASH_OFFSET_Y);
                destInventory = static_cast<inventory::InventoryType>(
                    static_cast<int>(inventory::InventoryType::SHARED_STASH_1) + currentStashIndex);
                validDrop = true;
            }

            if (validDrop)
            {
                // only send request if inventory or position changed
                if (destInventory != dragState.sourceInventory ||
                    targetPos.x != dragState.sourcePos.x ||
                    targetPos.y != dragState.sourcePos.y)
                {

                    std::cout << "Dropped item at ";
                    if (destInventory == inventory::InventoryType::PERSONAL)
                    {
                        std::cout << "personal inventory";
                    }
                    else
                    {
                        std::cout << "shared stash";
                    }
                    std::cout << " (" << targetPos.x << ", " << targetPos.y << ")" << std::endl;

                    // send MOVE_ITEM_REQUEST to server
                    client.requestMoveItem(
                        dragState.sourceInventory, dragState.sourcePos.x, dragState.sourcePos.y,
                        destInventory, targetPos.x, targetPos.y);
                }
                else
                {
                    std::cout << "Item returned to original position" << std::endl;
                }
            }
            else
            {
                std::cout << "Item dropped outside inventory, returning to source" << std::endl;
            }

            dragState.isDragging = false;
            dragState.draggedItem = nullptr;
        }

        BeginDrawing();
        ClearBackground(DARKGRAY);

        // drawing title
        DrawText("Inventory System", 10, 10, 20, BLACK);
        DrawText(TextFormat("User: %s", client.getUsername().c_str()), 10, 35, 16, BLACK);
        DrawText("Press ESC to exit | Keys 1-3: Switch Stash", 10, 55, 14, BLACK);

        // drawing shared stashes
        const int TAB_WIDTH = 80;
        const int TAB_HEIGHT = 25;
        const int TAB_Y = STASH_OFFSET_Y - 25;

        for (int i = 0; i < 3; ++i)
        {
            int tabX = STASH_OFFSET_X + i * (TAB_WIDTH + 5);
            Color tabColor = (i == currentStashIndex) ? DARKGRAY : LIGHTGRAY;
            Color textColor = (i == currentStashIndex) ? WHITE : BLACK;

            DrawRectangle(tabX, TAB_Y, TAB_WIDTH, TAB_HEIGHT, tabColor);
            DrawRectangleLines(tabX, TAB_Y, TAB_WIDTH, TAB_HEIGHT, BLACK);
            DrawText(TextFormat("Stash %d", i + 1), tabX + 15, TAB_Y + 5, 14, textColor);
        }

        // drawing the shared stash to the left
        drawInventoryGrid(12, 12, STASH_OFFSET_X, STASH_OFFSET_Y);

        // draw the items inside the selcted shared stash tab
        auto sharedStash = client.getSharedStash(currentStashIndex);
        auto currentStashType = static_cast<inventory::InventoryType>(
            static_cast<int>(inventory::InventoryType::SHARED_STASH_1) + currentStashIndex);
        drawInventoryItems(sharedStash, STASH_OFFSET_X, STASH_OFFSET_Y, &dragState, currentStashType, &iconCache);

        // drawing the personal inventory bottom right
        DrawText("Personal Inventory", INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y - 25, 16, BLACK);
        drawInventoryGrid(12, 5, INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y, &hoveredSlot);

        // draw items inside personal inventory
        auto personalInv = client.getPersonalInventory();
        drawInventoryItems(personalInv, INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y, &dragState, inventory::InventoryType::PERSONAL, &iconCache);

        // dragged item following the cursor
        if (dragState.isDragging && dragState.draggedItem)
        {
            auto size = dragState.draggedItem->getSize();
            int itemWidth = size.width * SLOT_SIZE + (size.width - 1) * SLOT_PADDING;
            int itemHeight = size.height * SLOT_SIZE + (size.height - 1) * SLOT_PADDING;

            int dragX = mouseX - dragState.mouseOffsetX;
            int dragY = mouseY - dragState.mouseOffsetY;

            // adding some transparency to the icon
            bool iconDrawn = false;
            std::string iconPath = getItemIconPath(dragState.draggedItem->getId());

            if (!iconPath.empty() && iconCache.find(iconPath) != iconCache.end())
            {
                Texture2D &texture = iconCache[iconPath];
                if (texture.id != 0)
                {
                    Rectangle source = {0, 0, (float)texture.width, (float)texture.height};
                    Rectangle dest = {(float)(dragX + 2), (float)(dragY + 2),
                                      (float)(itemWidth - 4), (float)(itemHeight - 4)};
                    DrawTexturePro(texture, source, dest, Vector2{0, 0}, 0.0f, Fade(WHITE, 0.6f));
                    iconDrawn = true;
                }
            }

            // fallback - no icon
            if (!iconDrawn)
            {
                Color dragColor = BLUE;
                if (size.width * size.height >= 4)
                {
                    dragColor = PURPLE;
                }
                else if (dragState.draggedItem->getStackLimit() > 10)
                {
                    dragColor = GOLD;
                }

                DrawRectangle(dragX + 2, dragY + 2, itemWidth - 4, itemHeight - 4,
                              Fade(dragColor, 0.5f));
            }

            DrawRectangleLines(dragX + 1, dragY + 1, itemWidth - 2, itemHeight - 2, WHITE);

            // drawing the item name while dragging
            std::string name = dragState.draggedItem->getName();
            if (name.length() > 8)
            {
                name = name.substr(0, 7) + ".";
            }
            DrawText(name.c_str(), dragX + 4, dragY + 4, 10, WHITE);

            // stack count
            if (dragState.stackCount > 1)
            {
                std::string countStr = std::to_string(dragState.stackCount);
                int countTextWidth = MeasureText(countStr.c_str(), 14);
                DrawText(countStr.c_str(),
                         dragX + itemWidth - countTextWidth - 4,
                         dragY + itemHeight - 18,
                         14, YELLOW);
            }
        }

        // split item dialog
        if (splitDialog.active)
        {
            // overlay
            DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.5f));

            // modal
            int dialogW = 400;
            int dialogH = 200;
            int dialogX = (SCREEN_WIDTH - dialogW) / 2;
            int dialogY = (SCREEN_HEIGHT - dialogH) / 2;

            DrawRectangle(dialogX, dialogY, dialogW, dialogH, Fade(LIGHTGRAY, 0.85f));
            DrawRectangleLines(dialogX, dialogY, dialogW, dialogH, Fade(BLACK, 0.85f));

            DrawText("Split Stack", dialogX + 20, dialogY + 20, 20, BLACK);

            if (splitDialog.item)
            {
                DrawText(TextFormat("Item: %s", splitDialog.item->getName().c_str()),
                         dialogX + 20, dialogY + 50, 16, DARKGRAY);
                DrawText(TextFormat("Max amount: %u", splitDialog.maxAmount),
                         dialogX + 20, dialogY + 70, 16, DARKGRAY);
            }

            // user input field
            DrawText("Amount to split:", dialogX + 20, dialogY + 100, 16, BLACK);
            DrawRectangle(dialogX + 20, dialogY + 120, 150, 30, WHITE);
            DrawRectangleLines(dialogX + 20, dialogY + 120, 150, 30, BLACK);
            DrawText(splitDialog.inputBuffer, dialogX + 25, dialogY + 127, 16, BLACK);

            DrawText("Click destination slot or press ESC to cancel",
                     dialogX + 20, dialogY + 165, 14, DARKGRAY);

            // handle keyboard input
            int key = GetCharPressed();
            while (key > 0)
            {
                if (key >= '0' && key <= '9')
                {
                    int len = strlen(splitDialog.inputBuffer);
                    if (len < 15)
                    {
                        splitDialog.inputBuffer[len] = (char)key;
                        splitDialog.inputBuffer[len + 1] = '\0';
                    }
                }
                key = GetCharPressed();
            }

            // backspace
            if (IsKeyPressed(KEY_BACKSPACE))
            {
                int len = strlen(splitDialog.inputBuffer);
                if (len > 0)
                {
                    splitDialog.inputBuffer[len - 1] = '\0';
                }
            }

            // esc to cancel
            if (IsKeyPressed(KEY_ESCAPE))
            {
                splitDialog.active = false;
            }

            // mouse click to select destination (only inside personal inventory)
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
            {
                inventory::GridPosition destPos(-1, -1);
                bool validDest = false;

                if (mouseOverInventory)
                {
                    destPos = screenToInventoryGrid(mouseX, mouseY, INVENTORY_OFFSET_X, INVENTORY_OFFSET_Y);
                    validDest = true;
                }

                if (validDest)
                {
                    int amount = atoi(splitDialog.inputBuffer);
                    if (amount > 0 && amount <= static_cast<int>(splitDialog.maxAmount))
                    {
                        // send split request
                        client.requestSplitStack(
                            splitDialog.invType,
                            splitDialog.sourcePos.x, splitDialog.sourcePos.y,
                            amount,
                            destPos.x, destPos.y);
                        splitDialog.active = false;
                    }
                }
            }
        }

        // connection status
        if (client.isConnected())
        {
            DrawText("Connected", SCREEN_WIDTH - 120, 10, 16, GREEN);
        }
        else
        {
            DrawText("Disconnected", SCREEN_WIDTH - 140, 10, 16, RED);
        }

        EndDrawing();
    }

    // Cleanup
    // Unload all textures
    for (auto &[path, texture] : iconCache)
    {
        UnloadTexture(texture);
    }

    CloseWindow();
    client.disconnect();

    std::cout << "Client shutdown complete." << std::endl;

    return 0;
}
