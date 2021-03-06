#include "pch.h"
#include "OrientedVirtualizingPanel.h"

using namespace Marduk::Controls;

OrientedVirtualizingPanel::OrientedVirtualizingPanel()
{
    _timer = ref new Windows::UI::Xaml::DispatcherTimer();
    _timer->Interval = TimeSpan{ 10000 };
    _timer->Tick += ref new Windows::Foundation::EventHandler<Object ^>(this, &OrientedVirtualizingPanel::OnTick);

    auto mc = MeasureControl;
}

void OrientedVirtualizingPanel::OnTick(Object^ sender, Object^e)
{
    _timer->Stop();
    _isSkip = false;
    InvalidateMeasure();
    InvalidateArrange();
}

Size OrientedVirtualizingPanel::MeasureOverride(Size availableSize)
{
    if (_parentScrollView == nullptr)
    {
        _parentScrollView = dynamic_cast<WinCon::ScrollViewer^>(this->Parent);

        if (_parentScrollView != nullptr)
        {
            _parentScrollView->ViewChanging += ref new Windows::Foundation::EventHandler<WinCon::ScrollViewerViewChangingEventArgs ^>(this, &OrientedVirtualizingPanel::OnViewChanging);
            _sizeChangedToken = _parentScrollView->SizeChanged += ref new Windows::UI::Xaml::SizeChangedEventHandler(this, &OrientedVirtualizingPanel::OnSizeChanged);
        }
    }

    if (_parentScrollView == nullptr)
    {
        return Size(availableSize.Width, 0);
    }

    if (_parentScrollView->ViewportHeight == 0)
    {
        return Size(availableSize.Width, 0);
    }

    if (_layout == nullptr)
    {
        _layout = GetLayout(availableSize);
    }

    _itemAvailableSize = GetItemAvailableSize(availableSize);

    if (_requestRelayout || NeedRelayout(availableSize))
    {
        if (DelayMeasure && !_requestRelayout)
        {
            _isSkip = true;
        }

        _requestRelayout = true;
    }

    if (_isSkip)
    {
        _isSkip = true;
        _timer->Stop();
        _timer->Start();
        return _layout->LayoutSize;
    }

    if (_requestRelayout)
    {
        _requestRelayout = false;

        if (_requestShowItemIndex < 0)
        {
            int requestFirstVisableItemIndex = _firstRealizationItemIndex;
            int requestLastVisableItemIndex = _lastRealizationItemIndex;
            delete(std::vector<Object^>*)(void*)(_layout->GetVisableItems(VisualWindow{ _parentScrollView->VerticalOffset,_parentScrollView->ViewportHeight }, &requestFirstVisableItemIndex, &requestLastVisableItemIndex));
            _requestShowItemIndex = requestFirstVisableItemIndex;
        }

        Relayout(availableSize);
    }

    if (_requestShowItemIndex >= 0)
    {
        auto requestScrollViewOffset = MakeItemVisable(_requestShowItemIndex);

        if (_scrollViewOffsetCache != _scrollViewOffset)
        {
            _scrollViewOffsetCache = _scrollViewOffset;
            if (requestScrollViewOffset.Y != _scrollViewOffset.Y)
            {
                _timer->Start();
                return _layout->LayoutSize;
            }
        }

        _requestShowItemIndex = -1;
    }

    if (_scrollViewOffset.X < 0 || _scrollViewOffset.Y < 0)
    {
        _scrollViewOffset = Point(_parentScrollView->HorizontalOffset, _parentScrollView->VerticalOffset);
    }
    _requestWindow = GetVisibleWindow(_scrollViewOffset, Size(_parentScrollView->ViewportWidth, _parentScrollView->ViewportHeight));

    for (int i = _layout->UnitCount; i < Items->Size; i++)
    {
        if (_layout->FillWindow(_requestWindow))
        {
            break;
        }

        Size itemSize = MeasureItem(Items->GetAt(i), Size{ 0,0 });
        _layout->AddItem(i, Items->GetAt(i), itemSize);
    }

    if (!_layout->FillWindow(_requestWindow))
    {
        LoadMoreItems();
    }

    int requestFirstRealizationItemIndex = _firstRealizationItemIndex;
    int requestLastRealizationItemIndex = _lastRealizationItemIndex;

    auto visableItems = (std::vector<Object^>*)(void*)(_layout->GetVisableItems(_requestWindow, &requestFirstRealizationItemIndex, &requestLastRealizationItemIndex));
    std::sort(_visableItems->begin(), _visableItems->end(), CompareObject());
    std::sort(visableItems->begin(), visableItems->end(), CompareObject());

    std::vector<Object^>* needRealizeItems = new std::vector<Object^>();
    std::vector<Object^>* needRecycleItems = new std::vector<Object^>();

    std::set_difference(visableItems->begin(), visableItems->end(), _visableItems->begin(), _visableItems->end(), std::back_inserter(*needRealizeItems), CompareObject());
    std::set_difference(_visableItems->begin(), _visableItems->end(), visableItems->begin(), visableItems->end(), std::back_inserter(*needRecycleItems), CompareObject());

    auto children = Children;

    for (auto item : *needRealizeItems)
    {
        auto container = RealizeItem(item);
        container->Measure(_itemAvailableSize);
    }

    for (auto item : *needRecycleItems)
    {
        RecycleItem(item);
    }

    delete(needRealizeItems);
    delete(needRecycleItems);
    delete(_visableItems);
    _visableItems = visableItems;


    _firstRealizationItemIndex = requestFirstRealizationItemIndex;
    _lastRealizationItemIndex = requestLastRealizationItemIndex;

    return _layout->LayoutSize;
}

Point OrientedVirtualizingPanel::MakeItemVisable(int index)
{
    auto rect = _layout->GetItemLayoutRect(index);
    _parentScrollView->ChangeView((double)rect.Left, (double)rect.Top, 1.0f, true);
    return Point{ rect.Left, rect.Top };
}

Size OrientedVirtualizingPanel::ArrangeOverride(Size finalSize)
{
    if (_isSkip)
    {
        return finalSize;
    }

    if (_layout == nullptr)
    {
        return finalSize;
    }

    if (_firstRealizationItemIndex < 0 || _lastRealizationItemIndex < 0)
    {
        return finalSize;
    }

    for (int i = _firstRealizationItemIndex; i <= _lastRealizationItemIndex; i++)
    {
        auto rect = _layout->GetItemLayoutRect(i);
        auto container = GetContainerFormIndex(i);
        container->Arrange(rect);
    }

    return finalSize;
}

void OrientedVirtualizingPanel::OnViewChanging(Object^ sender, WinCon::ScrollViewerViewChangingEventArgs ^ e)
{
    auto i = e->FinalView;
    int viewIndex = floor(e->NextView->VerticalOffset / (_parentScrollView->ViewportHeight / 2)) + 1;
    if (viewIndex != _viewIndex)
    {
        _viewIndex = viewIndex;
        _scrollViewOffset = Point(e->NextView->HorizontalOffset, e->NextView->VerticalOffset);
        InvalidateMeasure();
        InvalidateArrange();
    }
}

void OrientedVirtualizingPanel::OnSizeChanged(Object^ sender, Windows::UI::Xaml::SizeChangedEventArgs^ e)
{
    _parentScrollView->SizeChanged -= _sizeChangedToken;
    InvalidateMeasure();
    InvalidateArrange();
}

void OrientedVirtualizingPanel::OnItemsChanged(IObservableVector<Object^>^ source, IVectorChangedEventArgs^ e)
{
    if (_layout == nullptr)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }

    switch (e->CollectionChange)
    {
    case CollectionChange::Reset:
        _layout->RemoveAll();
        InvalidateMeasure();
        InvalidateArrange();
        break;
    case CollectionChange::ItemInserted:
        if (e->Index != Items->Size - 1)
        {
            if (e->Index <= _layout->UnitCount)
            {
                _layout->AddItem(e->Index, Items->GetAt(e->Index), MeasureItem(Items->GetAt(e->Index), Size(0, 0)));
            }
        }
        else
        {
            if (_layout->FillWindow(_requestWindow))
            {
                break;
            }
        }

        InvalidateMeasure();
        InvalidateArrange();
        break;
    case CollectionChange::ItemRemoved:
        if (e->Index < _layout->UnitCount)
        {
            _layout->RemoveItem(e->Index);

            InvalidateMeasure();
            InvalidateArrange();
        }
        break;
    case CollectionChange::ItemChanged:
        if (e->Index < _layout->UnitCount)
        {
            _layout->ChangeItem(e->Index, Items->GetAt(e->Index), MeasureItem(Items->GetAt(e->Index), Size(0, 0)));

            InvalidateMeasure();
            InvalidateArrange();
        }
        break;
    default:
        throw Exception::CreateException(-1, "Unexpected collection operation.");
        break;
    }
}

Size OrientedVirtualizingPanel::MeasureItem(Object^ item, Size oldSize)
{
    if (Resizer != nullptr && oldSize.Height > 0)
    {
        return Resizer->Resize(item, oldSize, _itemAvailableSize);
    }

    if (IsItemItsOwnContainerOverride(item))
    {
        auto measureControl = dynamic_cast<WinCon::ContentControl^>(item);
        Children->Append(measureControl);
        measureControl->Measure(_itemAvailableSize);
        auto result = measureControl->DesiredSize;
        Children->RemoveAtEnd();
        return result;
    }
    else
    {
        PrepareContainerForItemOverride(MeasureControl, item);
        MeasureControl->Measure(_itemAvailableSize);
        ClearContainerForItemOverride(MeasureControl, item);
        return MeasureControl->DesiredSize;
    }
}

VisualWindow OrientedVirtualizingPanel::GetVisibleWindow(Point offset, Size viewportSize)
{
    return  VisualWindow{ max(offset.Y - viewportSize.Height, 0),viewportSize.Height * 3 };
}

Size OrientedVirtualizingPanel::GetItemAvailableSize(Size availableSize)
{
    return availableSize;
}

bool OrientedVirtualizingPanel::NeedRelayout(Size availableSize)
{
    return _layout->Width != availableSize.Width;
}

void OrientedVirtualizingPanel::Relayout(Size availableSize)
{
    _layout->ChangePanelSize(availableSize.Width);

    for (int i = 0; i < _layout->UnitCount; i++)
    {
        auto newSize = MeasureItem(Items->GetAt(i), _layout->GetItemSize(i));
        _layout->ChangeItem(i, Items->GetAt(i), newSize);
    }
}

ILayout^ OrientedVirtualizingPanel::GetLayout(Size availableSize)
{
    return nullptr;
}