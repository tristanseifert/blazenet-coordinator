<?php

namespace App\View\Components;

use Illuminate\View\Component;

class Alert extends Component
{
    /**
     * Alert type class
     *
     * @var string
     */
    public $kindClass = "notification";

    /**
     * CSS class for the content wrapper
     *
     * @var string
     */
    public $contentClass = "";

    /**
     * Alert class (converted from type)
     *
     * @var string
     */
    public $cssClass;

    /**
     * Create a new component instance.
     *
     * @return void
     */
    public function __construct($type = null, $kind = null) {
        if($kind) {
            if($kind == "message") {
                $this->kindClass = "message";
                $this->contentClass = "message-body";
            }
        }

        switch(strtolower($type)) {
        case "error":
            $this->cssClass = "is-danger";
            break;
        case "warning":
            $this->cssClass = "is-warning";
            break;
        case "success":
            $this->cssClass = "is-success";
            break;
        case "info":
            $this->cssClass = "is-info";
            break;
        case "link":
            $this->cssClass = "is-link";
            break;
        default:
            $this->cssClass = "";
            break;
        }
    }

    /**
     * Get the view / contents that represent the component.
     *
     * @return \Illuminate\Contracts\View\View|\Closure|string
     */
    public function render()
    {
        return view('components.alert');
    }
}
