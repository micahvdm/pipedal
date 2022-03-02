// Copyright (c) 2022 Robin Davies
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

import React, { Component, ReactNode } from 'react';
import { WithStyles } from '@mui/styles';
import withStyles from '@mui/styles/withStyles';
import createStyles from '@mui/styles/createStyles';
import { Theme } from '@mui/material/styles';
import { Lv2Plugin } from './Lv2Plugin'



const pedalStyles = ({ palette }: Theme) => createStyles({
});

interface PedalProps extends WithStyles<typeof pedalStyles> {
    plugin: Lv2Plugin;
    children?: React.ReactChild | React.ReactChild[];

}

type PedalState = {
    
};

export const TemporaryDrawer = withStyles(pedalStyles)(
    class extends Component<PedalProps, PedalState>
    {
        state: PedalState;
    
        constructor(props: PedalProps) {
            super(props);
            this.state = { 
            };
        }
        render() : ReactNode {
            return (
                <div>
                    
                </div>
            );
        }

    }
);